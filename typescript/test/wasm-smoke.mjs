import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

import {
  Kind,
  Message,
  ProtoCacheWasmError,
  ProtoCacheWasmNotInitializedError,
  arraySchemaV1,
  compress,
  decompress,
  deserialize,
  init,
  mapSchemaV1,
  messageSchemaV1,
  serialize,
} from "../.test-dist/src/index.js";
import {
  decodeSchemaCount,
  nextClockNanoseconds,
  perfectHashBuild,
} from "../.test-dist/src/wasm-runtime.js";
import { loadTest } from "../.test-dist/test/generated/test.pc.js";

const encoder = new TextEncoder();

assert.equal(nextClockNanoseconds(1.25, -1n), 1_250_000n);
assert.equal(nextClockNanoseconds(1.25, 1_250_000n), 1_250_001n);
assert.equal(nextClockNanoseconds(1.0, 1_250_000n), 1_250_001n);

assert.throws(
  () => perfectHashBuild([]),
  ProtoCacheWasmNotInitializedError,
);
assert.throws(
  () => compress(new Uint8Array()),
  ProtoCacheWasmNotInitializedError,
);

const wasm = readFileSync(new URL("../dist/protocache.wasm", import.meta.url));
await assert.rejects(
  loadTest({ wasm: new Uint8Array() }),
  WebAssembly.CompileError,
);
const firstInitialization = init({ wasm });
assert.equal(init({ wasm: new Uint8Array() }), firstInitialization);
await firstInitialization;
const { Main, Small } = await loadTest({ wasm });

for (const rawKeys of [
  [],
  [""],
  ["alpha", "beta"],
  ...[24, 25, 255, 256].map((size) =>
    Array.from({ length: size }, (_, index) => `key-${index}`)),
  ["中文", "with\0nul", "a".repeat(200)],
]) {
  const keys = rawKeys.map((key) => encoder.encode(key));
  const result = perfectHashBuild(keys);
  assert.equal(result.slotToInput.length, keys.length);
  assert.equal(
    new DataView(
      result.index.buffer,
      result.index.byteOffset,
      result.index.byteLength,
    ).getUint32(0, true) & 0x0fff_ffff,
    keys.length,
  );
  assert.deepEqual(
    [...result.slotToInput].sort((left, right) => left - right),
    Array.from({ length: keys.length }, (_, index) => index),
  );
}

const seedKeys = [encoder.encode("seed-a"), encoder.encode("seed-b")];
const firstSeedIndex = perfectHashBuild(seedKeys).index;
const secondSeedIndex = perfectHashBuild(seedKeys).index;
assert.notEqual(
  new DataView(
    firstSeedIndex.buffer,
    firstSeedIndex.byteOffset,
    firstSeedIndex.byteLength,
  ).getUint32(4, true),
  new DataView(
    secondSeedIndex.buffer,
    secondSeedIndex.byteOffset,
    secondSeedIndex.byteLength,
  ).getUint32(4, true),
);

assert.throws(
  () => perfectHashBuild([encoder.encode("same"), encoder.encode("same")]),
  (error) =>
    error instanceof ProtoCacheWasmError &&
    error.status === 4,
);

for (const [schema, value] of [
  [
    mapSchemaV1(Kind.String, Kind.I32),
    new Map([
      ["", -1],
      ["中文", 2],
      ["long-key".repeat(8), 3],
      ["é", 4],
      ["e\u0301", 5],
    ]),
  ],
  [
    mapSchemaV1(Kind.I32, Kind.String),
    new Map([[-0x8000_0000, "min"], [0, "zero"], [0x7fff_ffff, "max"]]),
  ],
  [
    mapSchemaV1(Kind.U32, Kind.I32),
    new Map([[0, 1], [0xffff_ffff, 2]]),
  ],
  [
    mapSchemaV1(Kind.I64, Kind.I32),
    new Map([[-(1n << 63n), 1], [(1n << 63n) - 1n, 2]]),
  ],
  [
    mapSchemaV1(Kind.U64, Kind.I32),
    new Map([[0n, 1], [(1n << 64n) - 1n, 2]]),
  ],
]) {
  assert.deepEqual(
    deserialize(schema, serialize(schema, value)),
    value,
  );
}

for (const [schema, value] of [
  [arraySchemaV1(Kind.I32), []],
  [arraySchemaV1(Kind.I32), [-0x8000_0000, 0, 0x7fff_ffff]],
  [arraySchemaV1(Kind.String), ["", "中文", "with\0nul"]],
]) {
  assert.deepEqual(deserialize(schema, serialize(schema, value)), value);
}

const cppFixture = readFileSync(
  new URL("../.test-fixtures/test.pc", import.meta.url),
);
const damagedUtf8 = new Uint8Array(cppFixture);
const utf8Needle = encoder.encode("Hello World!");
let utf8Offset = -1;
for (
  let offset = 0;
  offset <= damagedUtf8.byteLength - utf8Needle.byteLength;
  offset += 1
) {
  if (utf8Needle.every((byte, index) => damagedUtf8[offset + index] === byte)) {
    utf8Offset = offset;
    break;
  }
}
assert.notEqual(utf8Offset, -1);
damagedUtf8[utf8Offset] = 0xff;
const schemaCountBeforeMain = decodeSchemaCount();
assert.throws(() => Main.deserialize(damagedUtf8), TypeError);
const mainSchemaCount = decodeSchemaCount();
const mainSchemaNodeCount = mainSchemaCount - schemaCountBeforeMain;
assert.ok(mainSchemaNodeCount > 1);
const smallBytes = new Small({ i32: 7 }).serialize();
assert.equal(Small.deserialize(smallBytes).i32, 7);
assert.equal(decodeSchemaCount(), mainSchemaCount);

let sharedFieldResolverCalls = 0;
class SharedFieldRoot extends Message {
  child = new Small();

  static schema;
}
SharedFieldRoot.schema = messageSchemaV1([
  [
    "child",
    0,
    false,
    Kind.None,
    Kind.Message,
    () => {
      sharedFieldResolverCalls += 1;
      return Small;
    },
  ],
]);
const sharedFieldValue = new SharedFieldRoot();
sharedFieldValue.child = new Small({ i32: 11 });
const sharedFieldBytes = serialize(SharedFieldRoot, sharedFieldValue);
assert.equal(sharedFieldResolverCalls, 1);
assert.equal(
  deserialize(SharedFieldRoot, sharedFieldBytes).child.i32,
  11,
);
assert.equal(sharedFieldResolverCalls, 1);

for (const input of [
  new Uint8Array(),
  encoder.encode("ProtoCache compression 世界"),
  Uint8Array.from({ length: 4096 }, (_, index) =>
    index % 11 === 0 ? 0xff : index % 7 === 0 ? 0 : index),
  cppFixture,
]) {
  assert.deepEqual(decompress(compress(input)), Uint8Array.from(input));
}

const slicedCompressionInput = new Uint8Array([99, 1, 2, 3, 88]).subarray(1, 4);
assert.deepEqual(
  decompress(compress(slicedCompressionInput)),
  Uint8Array.of(1, 2, 3),
);
if (typeof SharedArrayBuffer !== "undefined") {
  const shared = new SharedArrayBuffer(4);
  new Uint8Array(shared).set([4, 5, 6, 7]);
  assert.deepEqual(
    decompress(compress(new DataView(shared, 1, 2))),
    Uint8Array.of(5, 6),
  );
}
for (const invalid of [
  Uint8Array.of(0xff),
  Uint8Array.of(0xff, 0xff, 0xff, 0xff, 0x0f),
]) {
  assert.throws(
    () => decompress(invalid),
    (error) =>
      error instanceof ProtoCacheWasmError &&
      error.status === 7,
  );
}

const cppRoot = Main.deserialize(cppFixture);
const retainedData = Uint8Array.from(cppRoot.data);
const roundTrip = Main.deserialize(cppRoot.serialize());
assert.deepEqual(cppRoot.data, retainedData);
assert.deepEqual(roundTrip.index, cppRoot.index);
assert.deepEqual(
  [...roundTrip.objects]
    .map(([key, value]) => [key, value.i32, value.str])
    .sort(([left], [right]) => left - right),
  [...cppRoot.objects]
    .map(([key, value]) => [key, value.i32, value.str])
    .sort(([left], [right]) => left - right),
);
assert.deepEqual(roundTrip.matrix, cppRoot.matrix);
assert.deepEqual(roundTrip.vector, cppRoot.vector);
assert.deepEqual(roundTrip.arrays, cppRoot.arrays);

assert.throws(
  () => Main.deserialize(cppFixture.subarray(0, cppFixture.length - 4)),
  (error) => error instanceof RangeError || error instanceof TypeError,
);

const canonicalEmptyMessage = Main.deserialize(
  new Main({ object: new Small() }).serialize(),
);
assert.equal(canonicalEmptyMessage.object, undefined);

const largeRoot = new Main({
  str: "ProtoCache-😀-".repeat(8000),
  data: new Uint8Array(80 * 1024).fill(0xa5),
});
const largeRoundTrip = Main.deserialize(largeRoot.serialize());
assert.equal(largeRoundTrip.str, largeRoot.str);
assert.deepEqual(largeRoundTrip.data, largeRoot.data);

assert.throws(() => new Main({ i32: 0.5 }).serialize(), RangeError);
assert.throws(() => new Main({ str: "\ud800" }).serialize(), TypeError);
assert.throws(
  () => new Main({ data: [] }).serialize(),
  TypeError,
);
const cyclicRoot = new Main();
cyclicRoot.matrix = [];
cyclicRoot.matrix.push(cyclicRoot.matrix);
assert.throws(() => cyclicRoot.serialize(), TypeError);
const reentrantRoot = new Main();
Object.defineProperty(reentrantRoot, "str", {
  get() {
    reentrantRoot.serialize();
    return "unreachable";
  },
});
assert.throws(
  () => reentrantRoot.serialize(),
  /not reentrant/,
);

// Force WebAssembly.Memory.grow() and verify that the wrapper refreshes its
// TypedArray/DataView instances before it reads the native result.
const largeKey = new Uint8Array(17 * 1024 * 1024);
largeKey[0] = 1;
largeKey[largeKey.length - 1] = 2;
const largeResult = perfectHashBuild([largeKey]);
assert.deepEqual([...largeResult.slotToInput], [0]);

// The previous allocation grows WebAssembly.Memory. Reusing the serializer
// must refresh its cached views while retaining the native arena allocation.
const afterGrowth = Main.deserialize(cppRoot.serialize());
assert.equal(afterGrowth.i32, cppRoot.i32);
assert.deepEqual(afterGrowth.index, cppRoot.index);

const schemaCountBeforeRepeatedInit = decodeSchemaCount();
assert.equal(init({ wasm: new Uint8Array() }), firstInitialization);
const afterRepeatedInitBytes = cppRoot.serialize();
const afterRepeatedInit = Main.deserialize(afterRepeatedInitBytes);
assert.equal(afterRepeatedInit.i32, cppRoot.i32);
assert.deepEqual(afterRepeatedInit.index, cppRoot.index);
assert.equal(decodeSchemaCount(), schemaCountBeforeRepeatedInit);

console.log("Real WASM codec and PerfectHash smoke test: OK");
