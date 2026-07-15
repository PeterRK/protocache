import { compiledMessageFields } from "./compiled-fields.js";
import { compiledSchemaHandle } from "./compiled-schema.js";
import { Kind, type ValueKind } from "./kind.js";
import type { Message } from "./model.js";
import {
  resolveRuntimeType,
  type ArraySchema,
  type MapSchema,
  type MessageConstructor,
  type RuntimeType,
} from "./schema.js";
import {
  acquireSerializationArena,
  finishSerializationArena,
  type SerializationArenaRange,
} from "./wasm-runtime.js";

const INITIAL_CAPACITY = 64 * 1024;
const MAX_DEPTH = 256;
const MAX_BYTES = 1 << 30;
const MAX_DIRECT_ASCII_LENGTH = 128;
const CYCLE_SET_THRESHOLD = 16;
const textEncoder = new TextEncoder();

interface Context {
  readonly stack: object[];
  set: WeakSet<object> | undefined;
}

type ArenaWriter = (
  arena: PackedArena,
  value: unknown,
  context: Context,
  depth: number,
) => number | void;

class PackedArena {
  memory: WebAssembly.Memory;
  pointer: number;
  capacity: number;
  bytes!: Uint8Array;
  view!: DataView;
  offset = 0;

  constructor(range: SerializationArenaRange) {
    this.memory = range.memory;
    this.pointer = range.pointer;
    this.capacity = range.capacity;
    this.rebind();
  }

  reset(range: SerializationArenaRange): void {
    this.memory = range.memory;
    this.pointer = range.pointer;
    this.capacity = range.capacity;
    this.offset = 0;
    this.rebind();
  }

  resetCached(): void {
    this.offset = 0;
    if (this.bytes.buffer !== this.memory.buffer) this.rebind();
  }

  private rebind(): void {
    this.bytes = new Uint8Array(
      this.memory.buffer,
      this.pointer,
      this.capacity,
    );
    this.view = new DataView(
      this.memory.buffer,
      this.pointer,
      this.capacity,
    );
  }

  require(byteLength: number): void {
    const required = this.offset + byteLength;
    if (
      !Number.isSafeInteger(required) || required < 0 ||
      required >= MAX_BYTES
    ) {
      throw new RangeError("ProtoCache serialization arena is too large");
    }
    if (required <= this.capacity) return;
    const range = acquireSerializationArena(required);
    this.memory = range.memory;
    this.pointer = range.pointer;
    this.capacity = range.capacity;
    this.rebind();
  }

  begin(kind: number, auxiliary: number, count: number): number {
    this.require(16);
    const start = this.offset;
    this.view.setUint32(start, kind, true);
    this.view.setUint32(start + 4, auxiliary, true);
    this.view.setUint32(start + 8, count, true);
    this.view.setUint32(start + 12, 16, true);
    this.offset += 16;
    return start;
  }

  finish(start: number): void {
    const aligned = (this.offset + 3) & ~3;
    this.require(aligned - this.offset);
    this.offset = aligned;
    this.view.setUint32(start + 12, this.offset - start, true);
  }

  writeU32(value: number): void {
    this.require(4);
    this.view.setUint32(this.offset, value, true);
    this.offset += 4;
  }
}

let packedArena: PackedArena | undefined;
let serializationActive = false;
const serializationContext: Context = { stack: [], set: undefined };

function currentArena(): PackedArena {
  if (packedArena === undefined) {
    const range = acquireSerializationArena(INITIAL_CAPACITY);
    packedArena = new PackedArena(range);
  } else {
    packedArena.resetCached();
  }
  return packedArena;
}

function nextDepth(depth: number): number {
  if (depth >= MAX_DEPTH) {
    throw new RangeError("ProtoCache nesting depth exceeds the runtime limit");
  }
  return depth + 1;
}

function enterCycle(value: object, context: Context): void {
  const stack = context.stack;
  if (context.set === undefined) {
    for (let index = stack.length - 1; index >= 0; index -= 1) {
      if (stack[index] === value) {
        throw new TypeError("ProtoCache cannot serialize a cyclic object graph");
      }
    }
    if (stack.length === CYCLE_SET_THRESHOLD) {
      context.set = new WeakSet(stack);
      context.set.add(value);
    }
  } else {
    if (context.set.has(value)) {
      throw new TypeError("ProtoCache cannot serialize a cyclic object graph");
    }
    context.set.add(value);
  }
  stack.push(value);
}

function leaveCycle(value: object, context: Context): void {
  context.stack.pop();
  if (context.set !== undefined) {
    context.set.delete(value);
    if (context.stack.length < CYCLE_SET_THRESHOLD) context.set = undefined;
  }
}

function requireNumber(value: unknown, context: string): number {
  if (typeof value !== "number") {
    throw new TypeError(`${context} must be a number`);
  }
  return value;
}

function requireInteger(
  value: unknown,
  minimum: number,
  maximum: number,
  context: string,
): number {
  if (
    typeof value !== "number" || !Number.isInteger(value) ||
    value < minimum || value > maximum
  ) {
    throw new RangeError(`${context} must be an integer in [${minimum}, ${maximum}]`);
  }
  return value;
}

function requireBigInt(
  value: unknown,
  minimum: bigint,
  maximum: bigint,
  context: string,
): bigint {
  if (typeof value !== "bigint") {
    throw new TypeError(`${context} must be a bigint`);
  }
  if (value < minimum || value > maximum) {
    throw new RangeError(`${context} is out of range`);
  }
  return value;
}

function validatedUtf8Length(value: string): number {
  let byteLength = 0;
  for (let index = 0; index < value.length; index += 1) {
    const first = value.charCodeAt(index);
    if (first >= 0xd800 && first <= 0xdbff) {
      const second = value.charCodeAt(index + 1);
      if (!(second >= 0xdc00 && second <= 0xdfff)) {
        throw new TypeError("ProtoCache string contains an isolated UTF-16 surrogate");
      }
      index += 1;
      byteLength += 4;
    } else if (first >= 0xdc00 && first <= 0xdfff) {
      throw new TypeError("ProtoCache string contains an isolated UTF-16 surrogate");
    } else if (first < 0x80) {
      byteLength += 1;
    } else if (first < 0x800) {
      byteLength += 2;
    } else {
      byteLength += 3;
    }
  }
  if (byteLength >= MAX_BYTES) {
    throw new RangeError("ProtoCache string is too large");
  }
  return byteLength;
}

function writeScalar(arena: PackedArena, kind: ValueKind, value: unknown): void {
  switch (kind) {
    case Kind.Bool:
      if (typeof value !== "boolean") {
        throw new TypeError("ProtoCache bool field must be a boolean");
      }
      break;
    case Kind.I32:
    case Kind.Enum:
      requireInteger(value, -0x8000_0000, 0x7fff_ffff, "ProtoCache i32 field");
      break;
    case Kind.U32:
      requireInteger(value, 0, 0xffff_ffff, "ProtoCache u32 field");
      break;
    case Kind.I64:
      requireBigInt(value, -(1n << 63n), (1n << 63n) - 1n, "ProtoCache i64 field");
      break;
    case Kind.U64:
      requireBigInt(value, 0n, (1n << 64n) - 1n, "ProtoCache u64 field");
      break;
    case Kind.F32:
      requireNumber(value, "ProtoCache f32 field");
      break;
    case Kind.F64:
      requireNumber(value, "ProtoCache f64 field");
      break;
    default:
      throw new TypeError(`Unsupported packed scalar kind: ${kind}`);
  }

  const start = arena.begin(kind, 0, 0);
  switch (kind) {
    case Kind.Bool:
      arena.view.setUint32(start + 8, value ? 1 : 0, true);
      break;
    case Kind.I32:
    case Kind.Enum:
      arena.view.setInt32(start + 8, value as number, true);
      break;
    case Kind.U32:
      arena.view.setUint32(start + 8, value as number, true);
      break;
    case Kind.I64:
      arena.view.setBigInt64(start + 8, value as bigint, true);
      break;
    case Kind.U64:
      arena.view.setBigUint64(start + 8, value as bigint, true);
      break;
    case Kind.F32:
      arena.view.setFloat32(start + 8, value as number, true);
      break;
    case Kind.F64:
      arena.view.setFloat64(start + 8, value as number, true);
  }
}

function writeBlob(arena: PackedArena, kind: ValueKind, value: unknown): void {
  let byteLength: number;
  if (kind === Kind.String) {
    if (typeof value !== "string") {
      throw new TypeError("ProtoCache string field must be a string");
    }
    if (value.length <= MAX_DIRECT_ASCII_LENGTH) {
      arena.require(16 + value.length);
      const start = arena.begin(kind, 0, 0);
      let index = 0;
      for (; index < value.length; index += 1) {
        const code = value.charCodeAt(index);
        if (code >= 0x80) break;
        arena.bytes[arena.offset] = code;
        arena.offset += 1;
      }
      if (index === value.length) {
        arena.view.setUint32(start + 8, value.length, true);
        arena.finish(start);
        return;
      }
      arena.offset = start;
    }
    arena.require(16 + value.length);
    byteLength = validatedUtf8Length(value);
  } else {
    if (!(value instanceof Uint8Array)) {
      throw new TypeError("ProtoCache bytes field must be a Uint8Array");
    }
    byteLength = value.byteLength;
    if (byteLength >= MAX_BYTES) {
      throw new RangeError("ProtoCache byte array is too large");
    }
    arena.require(16 + byteLength);
  }

  const start = arena.begin(kind, 0, 0);
  arena.require(byteLength);
  if (typeof value === "string") {
    const result = textEncoder.encodeInto(
      value,
      arena.bytes.subarray(arena.offset, arena.offset + byteLength),
    );
    if (result.read !== value.length || result.written !== byteLength) {
      throw new TypeError("TextEncoder did not consume the validated string");
    }
    arena.offset += result.written;
  } else {
    arena.bytes.set(value, arena.offset);
    arena.offset += byteLength;
  }
  arena.view.setUint32(start + 8, byteLength, true);
  arena.finish(start);
}

function writeScalarArray(
  arena: PackedArena,
  kind: ValueKind,
  value: unknown,
): void {
  if (!Array.isArray(value)) {
    throw new TypeError("ProtoCache repeated field must be an Array");
  }
  const start = arena.begin(Kind.Array, kind, value.length);
  const width = kind === Kind.Bool
    ? 1
    : kind === Kind.I64 || kind === Kind.U64 || kind === Kind.F64
      ? 8
      : 4;
  arena.require(value.length * width);
  for (const item of value) {
    switch (kind) {
      case Kind.Bool:
        if (typeof item !== "boolean") {
          throw new TypeError("ProtoCache bool array element must be a boolean");
        }
        arena.bytes[arena.offset] = item ? 1 : 0;
        break;
      case Kind.I32:
      case Kind.Enum:
        arena.view.setInt32(arena.offset, requireInteger(
          item, -0x8000_0000, 0x7fff_ffff, "ProtoCache i32 array element",
        ), true);
        break;
      case Kind.U32:
        arena.view.setUint32(arena.offset, requireInteger(
          item, 0, 0xffff_ffff, "ProtoCache u32 array element",
        ), true);
        break;
      case Kind.I64:
        arena.view.setBigInt64(arena.offset, requireBigInt(
          item, -(1n << 63n), (1n << 63n) - 1n,
          "ProtoCache i64 array element",
        ), true);
        break;
      case Kind.U64:
        arena.view.setBigUint64(arena.offset, requireBigInt(
          item, 0n, (1n << 64n) - 1n, "ProtoCache u64 array element",
        ), true);
        break;
      case Kind.F32:
        arena.view.setFloat32(
          arena.offset,
          requireNumber(item, "ProtoCache f32 array element"),
          true,
        );
        break;
      case Kind.F64:
        arena.view.setFloat64(
          arena.offset,
          requireNumber(item, "ProtoCache f64 array element"),
          true,
        );
        break;
      default:
        throw new TypeError("Unsupported ProtoCache scalar array kind");
    }
    arena.offset += width;
  }
  arena.finish(start);
}

function isScalarKind(kind: ValueKind): boolean {
  return kind >= Kind.F64 && kind <= Kind.Enum;
}

function writeKey(arena: PackedArena, kind: number, value: unknown): void {
  if (kind === Kind.String) writeBlob(arena, Kind.String, value);
  else writeScalar(arena, kind as ValueKind, value);
}

const compiledMessages = new WeakMap<object, ArenaWriter>();
const compiledContainers = new WeakMap<object, ArenaWriter>();

function compileValueWriter(
  kind: ValueKind,
  resolver: (() => RuntimeType) | undefined,
  omitEmptyMessage = false,
): ArenaWriter {
  if (kind === Kind.String || kind === Kind.Bytes) {
    return (arena, value) => writeBlob(arena, kind, value);
  }
  if (kind === Kind.Message) {
    let type: MessageConstructor<Message> | undefined;
    let write: ArenaWriter | undefined;
    return (arena, value, context, depth) => {
      if (write === undefined) {
        type = resolveRuntimeType(
          resolver,
          Kind.Message,
        ) as MessageConstructor<Message>;
        write = compileMessageWriter(type);
      }
      if (
        typeof value !== "object" || value === null ||
        Object.getPrototypeOf(value) !== type!.prototype
      ) {
        throw new TypeError(
          "ProtoCache message field must match its schema type exactly",
        );
      }
      const start = arena.offset;
      const count = write(arena, value, context, nextDepth(depth));
      if (omitEmptyMessage && count === 0) {
        arena.offset = start;
        arena.begin(Kind.None, 0, 0);
      }
    };
  }
  if (kind === Kind.Array) {
    let write: ArenaWriter | undefined;
    return (arena, value, context, depth) => {
      if (write === undefined) {
        const schema = resolveRuntimeType(resolver, Kind.Array) as ArraySchema;
        write = compileArrayWriter(schema.valueKind, schema.valueType);
      }
      write(arena, value, context, nextDepth(depth));
    };
  }
  if (kind === Kind.Map) {
    let schema: MapSchema | undefined;
    let writeValue: ArenaWriter | undefined;
    return (arena, value, context, depth) => {
      if (schema === undefined) {
        schema = resolveRuntimeType(resolver, Kind.Map) as MapSchema;
        writeValue = compileValueWriter(schema.valueKind, schema.valueType);
      }
      writeMap(
        arena, value, schema.keyKind, schema.valueKind, writeValue!,
        context, nextDepth(depth),
      );
    };
  }
  return (arena, value) => writeScalar(arena, kind, value);
}

function compileArrayWriter(
  kind: ValueKind,
  resolver: (() => RuntimeType) | undefined,
): ArenaWriter {
  if (isScalarKind(kind)) {
    return (arena, value) => writeScalarArray(arena, kind, value);
  }
  const writeElement = compileValueWriter(kind, resolver);
  return (arena, value, context, depth) => {
    if (!Array.isArray(value)) {
      throw new TypeError("ProtoCache repeated field must be an Array");
    }
    enterCycle(value, context);
    try {
      const start = arena.begin(Kind.Array, kind, value.length);
      for (const item of value) writeElement(arena, item, context, depth);
      arena.finish(start);
    } finally {
      leaveCycle(value, context);
    }
  };
}

function writeMap(
  arena: PackedArena,
  value: unknown,
  keyKind: number,
  valueKind: ValueKind,
  writeValue: ArenaWriter,
  context: Context,
  depth: number,
): void {
  if (!(value instanceof Map)) {
    throw new TypeError("ProtoCache map field must be a Map");
  }
  enterCycle(value, context);
  try {
    const start = arena.begin(
      Kind.Map,
      keyKind | (valueKind << 8),
      value.size,
    );
    for (const [key, item] of value) {
      writeKey(arena, keyKind, key);
      writeValue(arena, item, context, depth);
    }
    arena.finish(start);
  } finally {
    leaveCycle(value, context);
  }
}

function isDefault(
  repeated: boolean,
  keyKind: number,
  valueKind: ValueKind,
  value: unknown,
): boolean {
  if (value === undefined || value === null) return true;
  if (keyKind !== Kind.None) return value instanceof Map && value.size === 0;
  if (repeated) return Array.isArray(value) && value.length === 0;
  switch (valueKind) {
    case Kind.Bool:
      return value === false;
    case Kind.I32:
    case Kind.Enum:
    case Kind.U32:
    case Kind.F32:
    case Kind.F64:
      return typeof value === "number" && value === 0;
    case Kind.I64:
    case Kind.U64:
      return value === 0n;
    case Kind.String:
      return value === "";
    case Kind.Bytes:
      return value instanceof Uint8Array && value.byteLength === 0;
    case Kind.Array:
      return Array.isArray(value) && value.length === 0;
    case Kind.Map:
      return value instanceof Map && value.size === 0;
    case Kind.Message:
      return false;
  }
}

function compileMessageWriter(type: MessageConstructor<Message>): ArenaWriter {
  const cached = compiledMessages.get(type);
  if (cached !== undefined) return cached;
  const fields = compiledMessageFields(type).map((field) => {
    const { name, id, repeated, keyKind, valueKind, resolver } = field;
    const write = keyKind !== Kind.None
      ? (() => {
          const writeValue = compileValueWriter(valueKind, resolver);
          return (
            arena: PackedArena,
            value: unknown,
            context: Context,
            depth: number,
          ) => writeMap(
            arena, value, keyKind, valueKind, writeValue, context, depth,
          );
        })()
      : repeated
        ? compileArrayWriter(valueKind, resolver)
        : compileValueWriter(
            valueKind,
            resolver,
            valueKind === Kind.Message,
          );
    return { name, id, repeated, keyKind, valueKind, write };
  });
  const fieldCapacity = fields.reduce(
    (maximum, field) => Math.max(maximum, field.id + 1),
    1,
  );
  const write: ArenaWriter = (arena, value, context, depth) => {
    if (typeof value !== "object" || value === null) {
      throw new TypeError("ProtoCache message field must be an object");
    }
    enterCycle(value, context);
    try {
      const start = arena.begin(Kind.Message, fieldCapacity, 0);
      const record = value as Record<string, unknown>;
      let count = 0;
      for (const field of fields) {
        const fieldValue = record[field.name];
        if (isDefault(
          field.repeated, field.keyKind, field.valueKind, fieldValue,
        )) continue;
        arena.writeU32(field.id);
        field.write(arena, fieldValue, context, depth);
        count += 1;
      }
      arena.view.setUint32(start + 8, count, true);
      arena.finish(start);
      return count;
    } finally {
      leaveCycle(value, context);
    }
  };
  compiledMessages.set(type, write);
  return write;
}

function compileRoot(type: RuntimeType): ArenaWriter {
  const key = type as object;
  const cached = compiledContainers.get(key);
  if (cached !== undefined) return cached;
  let write: ArenaWriter;
  if (typeof type === "function") {
    write = compileMessageWriter(type);
  } else if (type.kind === Kind.Array) {
    write = compileArrayWriter(type.valueKind, type.valueType);
  } else if (type.kind === Kind.Map) {
    const writeValue = compileValueWriter(type.valueKind, type.valueType);
    write = (arena, value, context, depth) => writeMap(
      arena, value, type.keyKind, type.valueKind, writeValue, context, depth,
    );
  } else {
    throw new TypeError("Unsupported ProtoCache runtime type");
  }
  compiledContainers.set(key, write);
  return write;
}

export function serializeWithArena(
  type: RuntimeType,
  value: unknown,
): Uint8Array {
  if (serializationActive) {
    throw new TypeError("ProtoCache WASM serialization is not reentrant");
  }
  serializationActive = true;
  try {
    const schemaHandle = compiledSchemaHandle(type);
    if (typeof type === "function" && (
      typeof value !== "object" || value === null ||
      Object.getPrototypeOf(value) !== type.prototype
    )) {
      throw new TypeError(
        "ProtoCache root message must match its schema type exactly",
      );
    }
    const arena = currentArena();
    compileRoot(type)(arena, value, serializationContext, 0);
    return finishSerializationArena(schemaHandle, arena.pointer, arena.offset);
  } finally {
    serializationActive = false;
  }
}
