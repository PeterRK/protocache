import { inputBytes, type BinaryInput } from "./binary.js";

const ABI_VERSION = 5;
const RESULT_SIZE = 20;
const BYTES_RESULT_SIZE = 12;
const STATUS_OK = 0;

type WasmFunction = (...args: number[]) => number;

interface WasmExports {
  readonly memory: WebAssembly.Memory;
  readonly initialize: () => void;
  readonly abiVersion: WasmFunction;
  readonly seedInitialize: WasmFunction;
  readonly alloc: WasmFunction;
  readonly free: WasmFunction;
  readonly perfectHashBuild: WasmFunction;
  readonly perfectHashBuildRelease: WasmFunction;
  readonly compress: WasmFunction;
  readonly decompress: WasmFunction;
  readonly bytesRelease: WasmFunction;
  readonly arenaBuffer: WasmFunction;
  readonly arenaCapacity: WasmFunction;
  readonly arenaReserve: WasmFunction;
  readonly arenaSerialize: WasmFunction;
  readonly arenaOutput: WasmFunction;
  readonly decodeInputReserve: WasmFunction;
  readonly decodeInputCapacity: WasmFunction;
  readonly decodePlanReserve: WasmFunction;
  readonly decodeSchemaReserve: WasmFunction;
  readonly decodeSchemaCount: WasmFunction;
  readonly decodePlanCommit: WasmFunction;
  readonly decodeTape: WasmFunction;
  readonly decodeTapeOutput: WasmFunction;
  readonly decodeLastError: WasmFunction;
  readonly lastError: WasmFunction;
}

interface Runtime {
  readonly exports: WasmExports;
}

export type WasmSource =
  | WebAssembly.Module
  | ArrayBuffer
  | ArrayBufferView;

export interface InitOptions {
  readonly wasm?: WasmSource;
  readonly wasmUrl?: string | URL;
}

interface PerfectHashBuildResult {
  readonly index: Uint8Array;
  readonly slotToInput: Uint32Array;
}

export interface SerializationArenaRange {
  readonly memory: WebAssembly.Memory;
  readonly pointer: number;
  readonly capacity: number;
}

interface DecodeTapeRange {
  readonly memory: WebAssembly.Memory;
  readonly inputPointer: number;
  readonly inputLength: number;
  readonly tapePointer: number;
  readonly tapeLength: number;
}

export class ProtoCacheWasmNotInitializedError extends Error {
  constructor() {
    super("ProtoCache WASM is not initialized; call and await init() first");
    this.name = "ProtoCacheWasmNotInitializedError";
  }
}

export class ProtoCacheWasmError extends Error {
  constructor(
    message: string,
    readonly status: number,
  ) {
    super(`${message} (status ${status})`);
    this.name = "ProtoCacheWasmError";
  }
}

let runtime: Runtime | undefined;

function requireFunction(
  exports: WebAssembly.Exports,
  name: string,
): WasmFunction {
  const value = exports[name];
  if (typeof value !== "function") {
    throw new TypeError(`ProtoCache WASM export ${name} is missing`);
  }
  return value as WasmFunction;
}

function checkedExports(instance: WebAssembly.Instance): WasmExports {
  const memory = instance.exports.memory;
  if (!(memory instanceof WebAssembly.Memory)) {
    throw new TypeError("ProtoCache WASM memory export is missing");
  }
  const initialize = instance.exports._initialize;
  if (typeof initialize !== "function") {
    throw new TypeError("ProtoCache WASM _initialize export is missing");
  }
  return {
    memory,
    initialize: initialize as () => void,
    abiVersion: requireFunction(instance.exports, "pc_wasm_abi_version"),
    seedInitialize: requireFunction(instance.exports, "pc_seed_initialize"),
    alloc: requireFunction(instance.exports, "pc_alloc"),
    free: requireFunction(instance.exports, "pc_free"),
    perfectHashBuild: requireFunction(instance.exports, "pc_ph_build"),
    perfectHashBuildRelease: requireFunction(
      instance.exports,
      "pc_ph_build_release",
    ),
    compress: requireFunction(instance.exports, "pc_compress"),
    decompress: requireFunction(instance.exports, "pc_decompress"),
    bytesRelease: requireFunction(instance.exports, "pc_bytes_release"),
    arenaBuffer: requireFunction(instance.exports, "pc_arena_buffer"),
    arenaCapacity: requireFunction(instance.exports, "pc_arena_capacity"),
    arenaReserve: requireFunction(instance.exports, "pc_arena_reserve"),
    arenaSerialize: requireFunction(instance.exports, "pc_arena_serialize"),
    arenaOutput: requireFunction(instance.exports, "pc_arena_output"),
    decodeInputReserve: requireFunction(
      instance.exports,
      "pc_decode_input_reserve",
    ),
    decodeInputCapacity: requireFunction(
      instance.exports,
      "pc_decode_input_capacity",
    ),
    decodePlanReserve: requireFunction(
      instance.exports,
      "pc_decode_plan_reserve",
    ),
    decodeSchemaReserve: requireFunction(
      instance.exports,
      "pc_decode_schema_reserve",
    ),
    decodeSchemaCount: requireFunction(
      instance.exports,
      "pc_decode_schema_count",
    ),
    decodePlanCommit: requireFunction(
      instance.exports,
      "pc_decode_plan_commit",
    ),
    decodeTape: requireFunction(instance.exports, "pc_decode_tape"),
    decodeTapeOutput: requireFunction(
      instance.exports,
      "pc_decode_tape_output",
    ),
    decodeLastError: requireFunction(
      instance.exports,
      "pc_decode_last_error",
    ),
    lastError: requireFunction(instance.exports, "pc_last_error"),
  };
}

function sourceBytes(source: ArrayBuffer | ArrayBufferView): ArrayBuffer {
  const bytes = ArrayBuffer.isView(source)
    ? new Uint8Array(source.buffer, source.byteOffset, source.byteLength)
    : new Uint8Array(source);
  return Uint8Array.from(bytes).buffer;
}

export function nextClockNanoseconds(
  milliseconds: number,
  previous: bigint,
): bigint {
  const observed = BigInt(Math.max(0, Math.floor(milliseconds * 1_000_000)));
  return observed > previous ? observed : previous + 1n;
}

async function loadModule(options: InitOptions): Promise<WebAssembly.Module> {
  if (options.wasm !== undefined && options.wasmUrl !== undefined) {
    throw new TypeError("Specify either wasm or wasmUrl, not both");
  }
  if (options.wasm instanceof WebAssembly.Module) {
    return options.wasm;
  }
  if (options.wasm !== undefined) {
    return WebAssembly.compile(sourceBytes(options.wasm));
  }
  if (options.wasmUrl !== undefined) {
    const response = await fetch(options.wasmUrl);
    if (!response.ok) {
      throw new Error(
        `Failed to load ProtoCache WASM: HTTP ${response.status}`,
      );
    }
    return WebAssembly.compile(await response.arrayBuffer());
  }
  throw new TypeError("ProtoCache init requires wasm or wasmUrl");
}

async function initialize(options: InitOptions): Promise<void> {
  const module = await loadModule(options);
  let memory: WebAssembly.Memory | undefined;
  let lastClockNanoseconds = -1n;
  const imports = {
    env: {
      emscripten_notify_memory_growth: (_index: number) => {},
    },
    wasi_snapshot_preview1: {
      clock_time_get: (
        _clockId: number,
        _precision: bigint,
        timePointer: number,
      ): number => {
        if (
          memory === undefined ||
          timePointer < 0 ||
          timePointer > memory.buffer.byteLength - 8
        ) {
          return 21;
        }
        const milliseconds =
          globalThis.performance?.now() ?? Date.now();
        const nanoseconds = nextClockNanoseconds(
          milliseconds,
          lastClockNanoseconds,
        );
        lastClockNanoseconds = nanoseconds;
        new DataView(memory.buffer).setBigUint64(
          timePointer,
          nanoseconds,
          true,
        );
        return 0;
      },
    },
  };
  const instance = await WebAssembly.instantiate(module, imports);
  const exports = checkedExports(instance);
  memory = exports.memory;
  exports.initialize();
  const actualAbi = exports.abiVersion() >>> 0;
  if (actualAbi !== ABI_VERSION) {
    throw new TypeError(
      `Unsupported ProtoCache WASM ABI ${actualAbi}; expected ${ABI_VERSION}`,
    );
  }
  const seed = new Uint32Array(1);
  if (
    typeof globalThis.crypto === "object" &&
    typeof globalThis.crypto.getRandomValues === "function"
  ) {
    globalThis.crypto.getRandomValues(seed);
  } else {
    seed[0] = (
      Math.floor(Math.random() * 0x1_0000_0000) ^
      Date.now() ^
      Math.floor((globalThis.performance?.now() ?? 0) * 1_000_000)
    ) >>> 0;
  }
  exports.seedInitialize(seed[0] ?? 0);
  runtime = { exports };
}

let initialization: Promise<void> | undefined;

export function ensureInitialized(options: InitOptions): Promise<void> {
  if (initialization !== undefined) return initialization;
  const pending = initialize(options);
  const guarded = pending.catch((error: unknown) => {
    if (initialization === guarded) initialization = undefined;
    throw error;
  });
  initialization = guarded;
  return guarded;
}

export function init(options: InitOptions): Promise<void> {
  return ensureInitialized(options);
}

function currentRuntime(): Runtime {
  if (runtime === undefined) {
    throw new ProtoCacheWasmNotInitializedError();
  }
  return runtime;
}

export function requireWasmInitialized(): void {
  currentRuntime();
}

function align8(value: number): number {
  return Math.ceil(value / 8) * 8;
}

function checkedU32(value: number, context: string): number {
  if (!Number.isSafeInteger(value) || value < 0 || value > 0xffff_ffff) {
    throw new RangeError(`${context} exceeds the WASM32 limit`);
  }
  return value;
}

function allocate(exports: WasmExports, size: number): number {
  const pointer = exports.alloc(checkedU32(size, "ProtoCache allocation")) >>> 0;
  if (pointer === 0) {
    throw new ProtoCacheWasmError("ProtoCache WASM allocation failed", 5);
  }
  return pointer;
}

function requireMemoryRange(
  memory: WebAssembly.Memory,
  pointer: number,
  length: number,
  context: string,
): void {
  if (
    !Number.isSafeInteger(pointer) ||
    !Number.isSafeInteger(length) ||
    pointer < 0 ||
    length < 0 ||
    pointer > memory.buffer.byteLength - length
  ) {
    throw new RangeError(`${context} returned an invalid WASM memory range`);
  }
}

export function acquireSerializationArena(
  minimumCapacity: number,
): SerializationArenaRange {
  const { exports } = currentRuntime();
  const minimum = checkedU32(
    minimumCapacity,
    "ProtoCache serialization arena",
  );
  if (minimum === 0) {
    throw new RangeError("ProtoCache serialization arena must not be empty");
  }
  const pointer = exports.arenaReserve(minimum) >>> 0;
  const capacity = exports.arenaCapacity() >>> 0;
  if (pointer === 0 || capacity < minimum) {
    throw new ProtoCacheWasmError(
      "ProtoCache serialization arena allocation failed",
      5,
    );
  }
  requireMemoryRange(
    exports.memory,
    pointer,
    capacity,
    "ProtoCache serialization arena",
  );
  return { memory: exports.memory, pointer, capacity };
}

export function reserveDecodeSchema(kind: number): number {
  const exports = currentRuntime().exports;
  const handle = exports.decodeSchemaReserve(kind) >>> 0;
  if (handle === 0) {
    throw new ProtoCacheWasmError(
      "ProtoCache decode schema allocation failed",
      exports.decodeLastError() >>> 0,
    );
  }
  return handle;
}

export function decodeSchemaCount(): number {
  return currentRuntime().exports.decodeSchemaCount() >>> 0;
}

export function commitDecodeSchema(
  schemaHandle: number,
  plan: Uint8Array,
): void {
  const { exports } = currentRuntime();
  const planLength = checkedU32(plan.byteLength, "ProtoCache decode plan");
  if (planLength === 0) {
    throw new RangeError("ProtoCache decode plan must not be empty");
  }
  const planPointer = exports.decodePlanReserve(planLength) >>> 0;
  if (planPointer === 0) {
    throw new ProtoCacheWasmError(
      "ProtoCache decode plan allocation failed",
      exports.decodeLastError() >>> 0,
    );
  }
  requireMemoryRange(
    exports.memory,
    planPointer,
    planLength,
    "ProtoCache decode plan",
  );
  new Uint8Array(exports.memory.buffer, planPointer, planLength).set(plan);
  const status = exports.decodePlanCommit(schemaHandle, planLength) >>> 0;
  if (status !== STATUS_OK) {
    throw new ProtoCacheWasmError(
      "ProtoCache decode plan was rejected",
      status,
    );
  }
}

export function buildDecodeTape(
  schemaHandle: number,
  input: Uint8Array,
): DecodeTapeRange {
  const { exports } = currentRuntime();
  const inputLength = checkedU32(input.byteLength, "ProtoCache decode input");
  if (inputLength === 0 || (inputLength & 3) !== 0) {
    throw new RangeError(
      "ProtoCache data must be a non-empty sequence of complete 32-bit words",
    );
  }

  const inputPointer = exports.decodeInputReserve(inputLength) >>> 0;
  const capacity = exports.decodeInputCapacity() >>> 0;
  if (inputPointer === 0 || capacity < inputLength) {
    throw new ProtoCacheWasmError(
      "ProtoCache decode input allocation failed",
      exports.decodeLastError() >>> 0,
    );
  }
  requireMemoryRange(
    exports.memory,
    inputPointer,
    inputLength,
    "ProtoCache decode input",
  );
  new Uint8Array(exports.memory.buffer, inputPointer, inputLength).set(input);

  const tapeLength = exports.decodeTape(schemaHandle, inputLength) >>> 0;
  if (tapeLength === 0) {
    const status = exports.decodeLastError() >>> 0;
    if (status === 2) {
      throw new RangeError("ProtoCache data contains an out-of-bounds range");
    }
    if (status === 3) {
      throw new RangeError("ProtoCache data contains an unaligned range");
    }
    if (status === 7) {
      throw new RangeError("ProtoCache data is malformed or truncated");
    }
    if (status === 1) {
      throw new TypeError("ProtoCache decode received an invalid argument");
    }
    throw new ProtoCacheWasmError("ProtoCache decode tape failed", status);
  }
  const tapePointer = exports.decodeTapeOutput() >>> 0;
  requireMemoryRange(
    exports.memory,
    inputPointer,
    inputLength,
    "ProtoCache decode input",
  );
  requireMemoryRange(
    exports.memory,
    tapePointer,
    tapeLength,
    "ProtoCache decode tape",
  );
  if ((tapePointer & 3) !== 0 || (tapeLength & 3) !== 0) {
    throw new RangeError("ProtoCache decode tape is not word-aligned");
  }
  return {
    memory: exports.memory,
    inputPointer,
    inputLength,
    tapePointer,
    tapeLength,
  };
}

export function finishSerializationArena(
  schemaHandle: number,
  pointer: number,
  length: number,
): Uint8Array {
  const { exports } = currentRuntime();
  checkedU32(pointer, "ProtoCache serialization arena pointer");
  checkedU32(length, "ProtoCache serialization arena length");
  requireMemoryRange(
    exports.memory,
    pointer,
    length,
    "ProtoCache serialization arena input",
  );
  const byteLength = exports.arenaSerialize(schemaHandle, pointer, length) >>> 0;
  if (byteLength === 0) {
    throw new ProtoCacheWasmError(
      "ProtoCache WASM serialization failed",
      6,
    );
  }
  const outputPointer = exports.arenaOutput() >>> 0;
  requireMemoryRange(
    exports.memory,
    outputPointer,
    byteLength,
    "ProtoCache serialization output",
  );
  return new Uint8Array(
    exports.memory.buffer,
    outputPointer,
    byteLength,
  ).slice();
}

function transformBytes(
  input: BinaryInput,
  operation: WasmFunction,
  context: string,
): Uint8Array {
  const { exports } = currentRuntime();
  const bytes = inputBytes(input);
  checkedU32(bytes.byteLength, `${context} input`);

  let inputPointer = 0;
  let resultPointer = 0;
  let allocationHandle = 0;
  try {
    if (bytes.byteLength !== 0) {
      inputPointer = allocate(exports, bytes.byteLength);
    }
    resultPointer = allocate(exports, BYTES_RESULT_SIZE);

    if (bytes.byteLength !== 0) {
      requireMemoryRange(
        exports.memory,
        inputPointer,
        bytes.byteLength,
        `${context} input`,
      );
      new Uint8Array(exports.memory.buffer).set(bytes, inputPointer);
    }

    const status = operation(
      inputPointer,
      bytes.byteLength,
      resultPointer,
    ) | 0;
    if (status !== STATUS_OK) {
      const detail = exports.lastError() >>> 0;
      throw new ProtoCacheWasmError(`${context} failed`, detail || status);
    }

    requireMemoryRange(
      exports.memory,
      resultPointer,
      BYTES_RESULT_SIZE,
      `${context} result`,
    );
    const result = new DataView(exports.memory.buffer);
    const dataPointer = result.getUint32(resultPointer, true);
    const dataLength = result.getUint32(resultPointer + 4, true);
    allocationHandle = result.getUint32(resultPointer + 8, true);
    if (allocationHandle === 0) {
      throw new TypeError(`${context} returned an invalid WASM result`);
    }
    requireMemoryRange(
      exports.memory,
      dataPointer,
      dataLength,
      `${context} output`,
    );
    return new Uint8Array(
      exports.memory.buffer,
      dataPointer,
      dataLength,
    ).slice();
  } finally {
    if (allocationHandle !== 0) {
      exports.bytesRelease(allocationHandle);
    }
    if (resultPointer !== 0) exports.free(resultPointer);
    if (inputPointer !== 0) exports.free(inputPointer);
  }
}

export function compress(input: BinaryInput): Uint8Array {
  const { exports } = currentRuntime();
  return transformBytes(input, exports.compress, "ProtoCache compression");
}

export function decompress(input: BinaryInput): Uint8Array {
  const { exports } = currentRuntime();
  return transformBytes(input, exports.decompress, "ProtoCache decompression");
}

export function perfectHashBuild(
  keys: readonly Uint8Array[],
): PerfectHashBuildResult {
  const { exports } = currentRuntime();
  if (keys.length >= 1 << 28) {
    throw new RangeError("ProtoCache Map has too many keys");
  }

  const offsets = new Uint32Array(keys.length);
  let blobLength = 0;
  for (let index = 0; index < keys.length; index += 1) {
    const key = keys[index];
    if (!(key instanceof Uint8Array)) {
      throw new TypeError("ProtoCache canonical Map key must be a Uint8Array");
    }
    offsets[index] = checkedU32(blobLength, "ProtoCache key blob");
    blobLength += Math.max(8, align8(key.byteLength));
    checkedU32(blobLength, "ProtoCache key blob");
  }

  let keysPointer = 0;
  let spansPointer = 0;
  let resultPointer = 0;
  let allocationHandle = 0;
  try {
    if (keys.length !== 0) {
      keysPointer = allocate(exports, blobLength);
      spansPointer = allocate(exports, keys.length * 8);
    }
    resultPointer = allocate(exports, RESULT_SIZE);

    if (keys.length !== 0) {
      let memoryBytes = new Uint8Array(exports.memory.buffer);
      requireMemoryRange(
        exports.memory,
        keysPointer,
        blobLength,
        "ProtoCache key blob",
      );
      memoryBytes.fill(0, keysPointer, keysPointer + blobLength);
      for (let index = 0; index < keys.length; index += 1) {
        memoryBytes.set(keys[index]!, keysPointer + offsets[index]!);
      }

      const spans = new DataView(exports.memory.buffer);
      requireMemoryRange(
        exports.memory,
        spansPointer,
        keys.length * 8,
        "ProtoCache key spans",
      );
      for (let index = 0; index < keys.length; index += 1) {
        const offset = spansPointer + index * 8;
        spans.setUint32(offset, offsets[index]!, true);
        spans.setUint32(offset + 4, keys[index]!.byteLength, true);
      }
    }

    const status = exports.perfectHashBuild(
      keysPointer,
      blobLength,
      spansPointer,
      keys.length,
      0,
      resultPointer,
    ) | 0;
    if (status !== STATUS_OK) {
      const detail = exports.lastError() >>> 0;
      throw new ProtoCacheWasmError(
        "ProtoCache PerfectHash build failed",
        detail || status,
      );
    }

    const result = new DataView(exports.memory.buffer);
    requireMemoryRange(
      exports.memory,
      resultPointer,
      RESULT_SIZE,
      "ProtoCache PerfectHash result",
    );
    const indexPointer = result.getUint32(resultPointer, true);
    const indexLength = result.getUint32(resultPointer + 4, true);
    const permutationPointer = result.getUint32(resultPointer + 8, true);
    const slotCount = result.getUint32(resultPointer + 12, true);
    allocationHandle = result.getUint32(resultPointer + 16, true);
    if (slotCount !== keys.length || allocationHandle === 0) {
      throw new TypeError("ProtoCache WASM returned an invalid build result");
    }
    requireMemoryRange(
      exports.memory,
      indexPointer,
      indexLength,
      "ProtoCache PerfectHash index",
    );
    requireMemoryRange(
      exports.memory,
      permutationPointer,
      slotCount * 4,
      "ProtoCache PerfectHash permutation",
    );
    const index = new Uint8Array(
      exports.memory.buffer,
      indexPointer,
      indexLength,
    ).slice();
    const slotToInput = new Uint32Array(
      exports.memory.buffer,
      permutationPointer,
      slotCount,
    ).slice();
    return { index, slotToInput };
  } finally {
    if (allocationHandle !== 0) {
      exports.perfectHashBuildRelease(allocationHandle);
    }
    if (resultPointer !== 0) exports.free(resultPointer);
    if (spansPointer !== 0) exports.free(spansPointer);
    if (keysPointer !== 0) exports.free(keysPointer);
  }
}
