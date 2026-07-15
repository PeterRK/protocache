import { existsSync, readFileSync, statSync } from "node:fs";
import { performance } from "node:perf_hooks";

import * as flatbuffers from "flatbuffers";
import protobufjs from "protobufjs";

import {
  compress,
  decompress,
} from "../.test-dist/src/index.js";
import { perfectHashBuild } from "../.test-dist/src/wasm-runtime.js";
import { loadTest } from "../.test-dist/test/generated/test.pc.js";

const fixtures = new URL("../.benchmark-fixtures/", import.meta.url);
const sourceFixtures = new URL("../../test/benchmark/", import.meta.url);
const requestedGroup = process.env.BENCH_GROUP ?? "all";
function fixture(name) {
  const staged = new URL(name, fixtures);
  return existsSync(staged) ? staged : new URL(name, sourceFixtures);
}
const pcBytes = new Uint8Array(readFileSync(fixture("test.pc")));
const pbBytes = new Uint8Array(readFileSync(fixture("test.pb")));
const fbBytes = new Uint8Array(readFileSync(fixture("test.fb")));
const wasmBytes = new Uint8Array(
  readFileSync(new URL("../dist/protocache.wasm", import.meta.url)),
);

const protobufRoot = await protobufjs.load(
  fixture("test.proto").pathname,
);
const ProtobufMain = protobufRoot.lookupType("test.Main");
const needsFlatBuffers = ["all", "decode", "read", "walk"].includes(
  requestedGroup,
);
const flatBuffersGenerated = new URL(
  "../.benchmark-fixtures/flatbuffers-js/test_generated.js",
  import.meta.url,
);
const FlatBuffersMain = needsFlatBuffers && existsSync(flatBuffersGenerated)
  ? (await import(
      flatBuffersGenerated.href
    )).Main
  : undefined;

const compileStarted = performance.now();
const wasmModule = await WebAssembly.compile(wasmBytes);
const wasmCompileMs = performance.now() - compileStarted;
const initStarted = performance.now();
const { Main, Small } = await loadTest({ wasm: wasmModule });
const wasmInstantiateMs = performance.now() - initStarted;

const iterations = Number.parseInt(process.env.BENCH_N ?? "5000", 10);
const rounds = Number.parseInt(process.env.BENCH_ROUNDS ?? "5", 10);
const warmupIterations = Number.parseInt(
  process.env.BENCH_WARMUP_N ?? String(Math.min(iterations, 1000)),
  10,
);
const minimumRoundMs = Number.parseFloat(
  process.env.BENCH_MIN_ROUND_MS ?? "50",
);
const maximumIterations = Number.parseInt(
  process.env.BENCH_MAX_N ?? "10000000",
  10,
);
const selectedGroup = requestedGroup;
const selectedCase = process.env.BENCH_CASE;
const selectedCaseNames = selectedCase?.split(",").map((name) => name.trim());
const benchmarkGroups = new Set([
  "all",
  "decode",
  "read",
  "walk",
  "serialize",
  "perfect-hash",
  "compress",
  "decompress",
]);

if (
  !Number.isSafeInteger(iterations) || iterations <= 0 ||
  !Number.isSafeInteger(rounds) || rounds <= 0 ||
  !Number.isSafeInteger(warmupIterations) || warmupIterations < 0 ||
  !Number.isFinite(minimumRoundMs) || minimumRoundMs <= 0 ||
  !Number.isSafeInteger(maximumIterations) || maximumIterations <= 0 ||
  !benchmarkGroups.has(selectedGroup)
) {
  throw new RangeError(
    "BENCH_N, BENCH_ROUNDS, BENCH_WARMUP_N, BENCH_MIN_ROUND_MS and " +
    "BENCH_MAX_N must be valid values; BENCH_GROUP must name a benchmark group",
  );
}

function asChecksum(value) {
  if (typeof value === "bigint") {
    return Number(value & 0xffff_ffffn);
  }
  if (typeof value === "number") {
    return value >>> 0;
  }
  if (
    typeof value === "object" && value !== null &&
    typeof value.low === "number"
  ) {
    return value.low >>> 0;
  }
  throw new TypeError("Unsupported 64-bit benchmark value");
}

function touchSmall(value) {
  return value === undefined
    ? 0
    : value.i32 + Number(value.flag) + value.str.length;
}

function touchProtoCache(root) {
  let total = root.i32 + root.u32 + asChecksum(root.i64) + asChecksum(root.u64);
  total += Number(root.flag) + root.mode + root.str.length + root.data.length;
  total += Math.trunc(root.f32 * 1000) + Math.trunc(root.f64 * 1000);
  total += touchSmall(root.object);
  total += root.i32v.reduce((sum, value) => sum + value, 0);
  total += root.u64v.reduce((sum, value) => sum + asChecksum(value), 0);
  total += root.strv.reduce((sum, value) => sum + value.length, 0);
  total += root.datav.reduce((sum, value) => sum + value.length, 0);
  total += root.f32v.reduce((sum, value) => sum + Math.trunc(value * 1000), 0);
  total += root.f64v.reduce((sum, value) => sum + Math.trunc(value * 1000), 0);
  total += root.flags.reduce((sum, value) => sum + Number(value), 0);
  total += root.objectv.reduce((sum, value) => sum + touchSmall(value), 0);
  total += root.t_u32 + root.t_i32 + root.t_s32;
  total += asChecksum(root.t_u64) + asChecksum(root.t_i64) + asChecksum(root.t_s64);
  for (const [key, value] of root.index) total += key.length + value;
  for (const [key, value] of root.objects) total += key + touchSmall(value);
  for (const row of root.matrix) {
    for (const value of row) total += Math.trunc(value * 1000);
  }
  for (const map of root.vector) {
    for (const [key, values] of map) {
      total += key.length;
      for (const value of values) total += Math.trunc(value * 1000);
    }
  }
  for (const [key, values] of root.arrays) {
    total += key.length;
    for (const value of values) total += Math.trunc(value * 1000);
  }
  total += root.modev.reduce((sum, value) => sum + value, 0);
  return total;
}

function touchProtobufSmall(value) {
  return value === null || value === undefined
    ? 0
    : value.i32 + Number(value.flag) + value.str.length;
}

function protobufMap(value) {
  return value ?? {};
}

function touchProtobuf(root) {
  let total = root.i32 + root.u32 + asChecksum(root.i64) + asChecksum(root.u64);
  total += Number(root.flag) + root.mode + root.str.length + root.data.length;
  total += Math.trunc(root.f32 * 1000) + Math.trunc(root.f64 * 1000);
  total += touchProtobufSmall(root.object);
  total += root.i32v.reduce((sum, value) => sum + value, 0);
  total += root.u64v.reduce((sum, value) => sum + asChecksum(value), 0);
  total += root.strv.reduce((sum, value) => sum + value.length, 0);
  total += root.datav.reduce((sum, value) => sum + value.length, 0);
  total += root.f32v.reduce((sum, value) => sum + Math.trunc(value * 1000), 0);
  total += root.f64v.reduce((sum, value) => sum + Math.trunc(value * 1000), 0);
  total += root.flags.reduce((sum, value) => sum + Number(value), 0);
  total += root.objectv.reduce((sum, value) => sum + touchProtobufSmall(value), 0);
  total += root.tU32 + root.tI32 + root.tS32;
  total += asChecksum(root.tU64) + asChecksum(root.tI64) + asChecksum(root.tS64);
  for (const [key, value] of Object.entries(protobufMap(root.index))) {
    total += key.length + value;
  }
  for (const [key, value] of Object.entries(protobufMap(root.objects))) {
    total += Number(key) + touchProtobufSmall(value);
  }
  for (const row of root.matrix?._ ?? []) {
    for (const value of row._ ?? []) total += Math.trunc(value * 1000);
  }
  for (const map of root.vector ?? []) {
    for (const [key, values] of Object.entries(protobufMap(map._))) {
      total += key.length;
      for (const value of values._ ?? []) total += Math.trunc(value * 1000);
    }
  }
  for (const [key, values] of Object.entries(protobufMap(root.arrays?._))) {
    total += key.length;
    for (const value of values._ ?? []) total += Math.trunc(value * 1000);
  }
  total += (root.modev ?? []).reduce((sum, value) => sum + value, 0);
  return total;
}

function fbStringLength(value) {
  return value === null ? 0 : value.length;
}

function touchFlatBuffersSmall(value) {
  return value === null
    ? 0
    : value.i32() + Number(value.flag()) + fbStringLength(value.str());
}

function touchFlatFloatArray(value) {
  if (value === null) return 0;
  let total = 0;
  for (let index = 0; index < value._length(); index += 1) {
    total += Math.trunc(value._(index) * 1000);
  }
  return total;
}

function touchFlatArrMap(value) {
  if (value === null) return 0;
  let total = 0;
  for (let index = 0; index < value._length(); index += 1) {
    const entry = value._(index);
    total += fbStringLength(entry.key()) + touchFlatFloatArray(entry.value());
  }
  return total;
}

function touchFlatBuffers(root) {
  let total = root.i32() + root.u32() + asChecksum(root.i64()) + asChecksum(root.u64());
  total += Number(root.flag()) + root.mode() + fbStringLength(root.str()) + root.dataLength();
  total += Math.trunc(root.f32() * 1000) + Math.trunc(root.f64() * 1000);
  total += touchFlatBuffersSmall(root.object());
  for (let i = 0; i < root.i32vLength(); i += 1) total += root.i32v(i);
  for (let i = 0; i < root.u64vLength(); i += 1) total += asChecksum(root.u64v(i));
  for (let i = 0; i < root.strvLength(); i += 1) total += fbStringLength(root.strv(i));
  for (let i = 0; i < root.datavLength(); i += 1) total += root.datav(i)?._length() ?? 0;
  for (let i = 0; i < root.f32vLength(); i += 1) total += Math.trunc(root.f32v(i) * 1000);
  for (let i = 0; i < root.f64vLength(); i += 1) total += Math.trunc(root.f64v(i) * 1000);
  for (let i = 0; i < root.flagsLength(); i += 1) total += Number(root.flags(i));
  for (let i = 0; i < root.objectvLength(); i += 1) total += touchFlatBuffersSmall(root.objectv(i));
  total += root.tU32() + root.tI32() + root.tS32();
  total += asChecksum(root.tU64()) + asChecksum(root.tI64()) + asChecksum(root.tS64());
  for (let i = 0; i < root.indexLength(); i += 1) {
    const entry = root.index(i);
    total += fbStringLength(entry.key()) + entry.value();
  }
  for (let i = 0; i < root.objectsLength(); i += 1) {
    const entry = root.objects(i);
    total += entry.key() + touchFlatBuffersSmall(entry.value());
  }
  const matrix = root.matrix();
  if (matrix !== null) {
    for (let i = 0; i < matrix._length(); i += 1) total += touchFlatFloatArray(matrix._(i));
  }
  for (let i = 0; i < root.vectorLength(); i += 1) total += touchFlatArrMap(root.vector(i));
  total += touchFlatArrMap(root.arrays());
  return total;
}

function flatBuffersRoot() {
  if (FlatBuffersMain === undefined) {
    throw new Error("FlatBuffers fixture is unavailable for this benchmark group");
  }
  return FlatBuffersMain.getRootAsMain(new flatbuffers.ByteBuffer(fbBytes));
}

function median(values) {
  const sorted = [...values].sort((left, right) => left - right);
  const middle = Math.floor(sorted.length / 2);
  return sorted.length % 2 === 0
    ? (sorted[middle - 1] + sorted[middle]) / 2
    : sorted[middle];
}

function relativeStandardDeviation(values) {
  const mean = values.reduce((sum, value) => sum + value, 0) / values.length;
  if (mean === 0) return 0;
  const variance = values.reduce(
    (sum, value) => sum + (value - mean) ** 2,
    0,
  ) / values.length;
  return Math.sqrt(variance) / mean * 100;
}

function prepareBenchmark(name, operation, requestedCount = iterations) {
  let checksum = 0;
  for (let index = 0; index < warmupIterations; index += 1) {
    checksum = (checksum + operation()) >>> 0;
  }

  const calibrationCount = Math.max(1, Math.min(requestedCount, 1000));
  const calibrationStarted = performance.now();
  for (let index = 0; index < calibrationCount; index += 1) {
    checksum = (checksum + operation()) >>> 0;
  }
  const calibrationMs = Math.max(
    performance.now() - calibrationStarted,
    0.001,
  );
  const calibratedCount = Math.ceil(
    calibrationCount * minimumRoundMs / calibrationMs,
  );

  return {
    name,
    operation,
    count: Math.min(
      maximumIterations,
      Math.max(requestedCount, calibratedCount),
    ),
    checksum,
    samples: [],
    heapDeltas: [],
  };
}

function printBenchmark(state) {
  const best = Math.min(...state.samples);
  const middle = median(state.samples);
  console.log(
    `${state.name.padEnd(28)} best=${(best / 1000).toFixed(3).padStart(10)} us/op  ` +
    `median=${(middle / 1000).toFixed(3).padStart(10)} us/op  ` +
    `rsd=${relativeStandardDeviation(state.samples).toFixed(1).padStart(5)}%  ` +
    `n=${String(state.count).padStart(8)}  ` +
    `ops/s=${(1_000_000_000 / best).toFixed(1).padStart(10)}  ` +
    `heapΔ=${Math.max(...state.heapDeltas)}  checksum=${state.checksum}`,
  );
}

function benchmarkGroup(cases) {
  const states = cases.map(({ name, operation, count }) =>
    prepareBenchmark(name, operation, count)
  );
  for (let round = 0; round < rounds; round += 1) {
    for (let offset = 0; offset < states.length; offset += 1) {
      const state = states[(round + offset) % states.length];
      globalThis.gc?.();
      const heapBefore = process.memoryUsage().heapUsed;
      const started = performance.now();
      for (let index = 0; index < state.count; index += 1) {
        state.checksum = (state.checksum + state.operation()) >>> 0;
      }
      const elapsed = performance.now() - started;
      state.samples.push(elapsed * 1_000_000 / state.count);
      state.heapDeltas.push(process.memoryUsage().heapUsed - heapBefore);
    }
  }
  for (const state of states) printBenchmark(state);
}

function runGroup(name, cases) {
  if (selectedGroup !== "all" && selectedGroup !== name) return;
  const selected = selectedCaseNames === undefined
    ? cases
    : cases.filter(({ name: caseName }) => selectedCaseNames.includes(caseName));
  if (selected.length === 0) {
    throw new RangeError(
      `BENCH_CASE ${JSON.stringify(selectedCase)} did not match group ${name}`,
    );
  }
  console.log(`======== ${name} ========`);
  benchmarkGroup(selected);
}

const pcRoot = Main.deserialize(pcBytes);
const pcRootWithoutMaps = Main.deserialize(pcBytes);
pcRootWithoutMaps.index.clear();
pcRootWithoutMaps.objects.clear();
pcRootWithoutMaps.vector = [];
pcRootWithoutMaps.arrays.clear();
const simpleRoot = new Main({
  i32: -999,
  object: new Small({ i32: 1 }),
});
const largeRoot = new Main({
  str: "ProtoCache-😀-".repeat(8000),
  data: new Uint8Array(80 * 1024).fill(0xa5),
});
const simpleBytes = simpleRoot.serialize();
const largeBytes = largeRoot.serialize();
const pbRoot = ProtobufMain.decode(pbBytes);
const fbRoot = FlatBuffersMain === undefined ? undefined : flatBuffersRoot();
console.log(`node=${process.version}`);
console.log(
  `iterations=${iterations} rounds=${rounds} warmup=${warmupIterations} ` +
  `minRound=${minimumRoundMs}ms cpu=${process.env.BENCH_CPU ?? "unbound"} ` +
  `group=${selectedGroup}`,
);
console.log(
  `pc=${pcBytes.length}B pb=${pbBytes.length}B fb=${fbBytes.length}B ` +
  `wasm=${statSync(new URL("../dist/protocache.wasm", import.meta.url)).size}B`,
);
console.log(
  `baseline pc=${touchProtoCache(pcRoot)} pb=${touchProtobuf(pbRoot)} ` +
  `fb=${fbRoot === undefined ? "skipped" : touchFlatBuffers(fbRoot)}`,
);
console.log(
  `wasm compile=${wasmCompileMs.toFixed(3)}ms instantiate=${wasmInstantiateMs.toFixed(3)}ms`,
);

const decodeCases = [
  { name: "ProtoCache tape deserialize", operation: () => Main.deserialize(pcBytes).i32 },
  {
    name: "PC tape deserialize simple",
    operation: () => Main.deserialize(simpleBytes).i32,
  },
  {
    name: "PC tape deserialize large",
    operation: () => Main.deserialize(largeBytes).str.length,
    count: Math.max(1, Math.min(iterations, 2000)),
  },
  { name: "protobuf deserialize", operation: () => ProtobufMain.decode(pbBytes).i32 },
];
const readCases = [
  { name: "ProtoCache tape read+walk", operation: () => touchProtoCache(Main.deserialize(pcBytes)) },
  { name: "protobuf read+walk", operation: () => touchProtobuf(ProtobufMain.decode(pbBytes)) },
];
const walkCases = [
  { name: "ProtoCache walk", operation: () => touchProtoCache(pcRoot) },
  { name: "protobuf walk", operation: () => touchProtobuf(pbRoot) },
];
if (FlatBuffersMain !== undefined) {
  decodeCases.push({
    name: "FlatBuffers root view",
    operation: () => flatBuffersRoot().i32(),
  });
  readCases.push({
    name: "FlatBuffers read+walk",
    operation: () => touchFlatBuffers(flatBuffersRoot()),
  });
  walkCases.push({
    name: "FlatBuffers walk",
    operation: () => touchFlatBuffers(fbRoot),
  });
}
runGroup("decode", decodeCases);
runGroup("read", readCases);
runGroup("walk", walkCases);
runGroup("serialize", [
  { name: "ProtoCache arena serialize", operation: () => pcRoot.serialize().length },
  { name: "PC arena serialize no Map", operation: () => pcRootWithoutMaps.serialize().length },
  { name: "PC arena serialize simple", operation: () => simpleRoot.serialize().length },
  {
    name: "PC arena serialize large",
    operation: () => largeRoot.serialize().length,
    count: Math.max(1, Math.min(iterations, 2000)),
  },
  { name: "protobuf serialize", operation: () => ProtobufMain.encode(pbRoot).finish().length },
  {
    name: "PC construct+arena simple",
    operation: () => new Main({
      i32: -999,
      object: new Small({ i32: 1 }),
    }).serialize().length,
  },
]);

const encoder = new TextEncoder();
const perfectHashCases = [];
for (const size of [1, 25, 256]) {
  const keys = Array.from({ length: size }, (_, index) => encoder.encode(`key-${index}`));
  perfectHashCases.push({
    name: `string keys n=${size}`,
    operation: () => {
      const result = perfectHashBuild(keys);
      return result.index.length + result.slotToInput.length;
    },
    count: Math.max(1, Math.min(iterations, Math.floor(25_000 / size))),
  });
}
runGroup("perfect-hash", perfectHashCases);

const compressCases = [];
const decompressCases = [];
if (
  selectedGroup === "all" || selectedGroup === "compress" ||
  selectedGroup === "decompress"
) {
  console.log("======== compression sizes ========");
  for (const [name, raw] of [["pc", pcBytes], ["pb", pbBytes], ["fb", fbBytes]]) {
    const packed = compress(raw);
    if (!Buffer.from(decompress(packed)).equals(Buffer.from(raw))) {
      throw new Error(`${name} compression round trip changed the fixture`);
    }
    console.log(`${name}: raw=${raw.length}B compressed=${packed.length}B`);
    compressCases.push({
      name: `${name} compress`,
      operation: () => compress(raw).length,
    });
    decompressCases.push({
      name: `${name} decompress`,
      operation: () => decompress(packed).length,
    });
  }
}
runGroup("compress", compressCases);
runGroup("decompress", decompressCases);
