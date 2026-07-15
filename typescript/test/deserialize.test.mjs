import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import { testApi } from "./init-wasm.mjs";
import * as pc from "../.test-dist/src/index.js";

const { Main, Mode, Small } = testApi;

const fixture = new Uint8Array(
  readFileSync(new URL("../.test-fixtures/test.pc", import.meta.url)),
);
const textDecoder = new TextDecoder();

function sortedEntries(map) {
  return [...map.entries()].sort(([left], [right]) =>
    String(left).localeCompare(String(right)),
  );
}

test("fully materializes the shared C++ test.pc fixture", () => {
  const root = Main.deserialize(fixture);

  assert.equal(root instanceof Main, true);
  assert.equal(root.i32, -999);
  assert.equal(root.u32, 1234);
  assert.equal(root.i64, -9876543210n);
  assert.equal(root.u64, 98765432123456789n);
  assert.equal(root.flag, true);
  assert.equal(root.mode, Mode.MODE_C);
  assert.equal(root.str, "Hello World!");
  assert.equal(textDecoder.decode(root.data), "abc123!?$*&()'-=@~");
  assert.equal(Math.fround(root.f32), Math.fround(-2.1));
  assert.equal(root.f64, 1);

  assert.equal(root.object instanceof Small, true);
  assert.equal(root.object.i32, 88);
  assert.equal(root.object.flag, false);
  assert.equal(root.object.str, "tmp");

  assert.deepEqual(root.i32v, [1, 2]);
  assert.deepEqual(root.u64v, [12345678987654321n]);
  assert.deepEqual(root.strv, [
    "abc",
    "apple",
    "banana",
    "orange",
    "pear",
    "grape",
    "strawberry",
    "cherry",
    "mango",
    "watermelon",
  ]);
  assert.deepEqual(root.datav, []);
  assert.deepEqual(
    root.f32v.map((value) => Math.fround(value)),
    [Math.fround(1.1), Math.fround(2.2)],
  );
  assert.deepEqual(root.f64v, [9.9, 8.8, 7.7, 6.6, 5.5]);
  assert.deepEqual(root.flags, [true, true, false, true, false, false, false]);
  assert.deepEqual(
    root.objectv.map(({ i32, flag, str }) => ({ i32, flag, str })),
    [
      { i32: 1, flag: false, str: "" },
      { i32: 0, flag: true, str: "" },
      { i32: 0, flag: false, str: "good luck!" },
    ],
  );

  assert.equal(root.t_u32, 0);
  assert.equal(root.t_i32, 0);
  assert.equal(root.t_s32, 0);
  assert.equal(root.t_u64, 0n);
  assert.equal(root.t_i64, 0n);
  assert.equal(root.t_s64, 0n);

  assert.deepEqual(sortedEntries(root.index), [
    ["abc-1", 1],
    ["abc-2", 2],
    ["x-1", 1],
    ["x-2", 2],
    ["x-3", 3],
    ["x-4", 4],
  ]);
  assert.deepEqual(
    [...root.objects.entries()]
      .sort(([left], [right]) => left - right)
      .map(([key, value]) => [key, value.i32, value.str]),
    [
      [1, 1, "aaaaaaaaaaa"],
      [2, 2, "b"],
      [3, 3, "ccccccccccccccc"],
      [4, 4, "ddddd"],
    ],
  );

  assert.deepEqual(root.matrix, [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9],
  ]);
  assert.deepEqual(
    root.vector.map((map) =>
      sortedEntries(map).map(([key, value]) => [key, value]),
    ),
    [
      [
        ["lv1", [11, 12]],
        ["lv2", [21, 22]],
      ],
      [["lv3", [31, 32]]],
    ],
  );
  assert.deepEqual(sortedEntries(root.arrays), [
    ["lv4", [41, 42]],
    ["lv5", [51, 52]],
  ]);
  assert.deepEqual(root.modev, []);

  assert.equal(root.hasField("objectv"), true);
  assert.equal(root.hasField("t_i32"), false);
  assert.equal(root.hasField("modev"), false);
});

test("generic deserialize and inherited static deserialize agree", () => {
  const direct = pc.deserialize(Main, fixture);
  const inherited = Main.deserialize(fixture);

  assert.equal(direct.i32, inherited.i32);
  assert.deepEqual(direct.strv, inherited.strv);
  assert.deepEqual(sortedEntries(direct.index), sortedEntries(inherited.index));
});

test("stores decoded field presence on the message object", () => {
  const root = Main.deserialize(fixture);
  const symbols = Object.getOwnPropertySymbols(root);

  assert.equal(symbols.length, 1);
  assert.equal(symbols[0].description, "ProtoCache.presence");
  assert.equal(Object.prototype.propertyIsEnumerable.call(root, symbols[0]), true);
  assert.equal(root.hasField("object"), true);
});

test("accepts an unaligned ArrayBufferView and does not retain input bytes", () => {
  const padded = new Uint8Array(fixture.byteLength + 2);
  padded.set(fixture, 1);
  const slice = padded.subarray(1, 1 + fixture.byteLength);
  assert.equal(slice.byteOffset % 4, 1);

  const root = Main.deserialize(slice);
  const decodedData = new Uint8Array(root.data);
  slice.fill(0);

  assert.equal(root.str, "Hello World!");
  assert.deepEqual(root.data, decodedData);
  assert.equal(textDecoder.decode(root.data), "abc123!?$*&()'-=@~");
});

test("snapshots SharedArrayBuffer input", () => {
  const shared = new SharedArrayBuffer(fixture.byteLength);
  const source = new Uint8Array(shared);
  source.set(fixture);

  const root = Main.deserialize(source);
  source.fill(0);

  assert.equal(root.i32, -999);
  assert.equal(root.object.str, "tmp");
  assert.equal(root.index.get("x-4"), 4);
});

test("rejects truncated and incomplete-word input", () => {
  assert.throws(
    () => Main.deserialize(fixture.subarray(0, fixture.byteLength - 4)),
    RangeError,
  );
  assert.throws(
    () => Main.deserialize(fixture.subarray(0, fixture.byteLength - 1)),
    /complete 32-bit words/,
  );
});

test("caches CompiledSchema and resolves nested types once", () => {
  let resolves = 0;

  class ObjectOnly extends pc.Message {
    static schema;
  }
  ObjectOnly.schema = pc.messageSchemaV1([
    [
      "object",
      10,
      false,
      pc.Kind.None,
      pc.Kind.Message,
      () => {
        resolves += 1;
        return Small;
      },
    ],
  ]);

  assert.equal(ObjectOnly.deserialize(fixture).object.str, "tmp");
  assert.equal(ObjectOnly.deserialize(fixture).object.i32, 88);
  assert.equal(resolves, 1);
});

test("scans unknown fields and multiple message sections once", () => {
  class Wire extends pc.Message {
    first = 0;
    middle = 0;
    target = 0;

    static schema;

    constructor(init) {
      super();
      if (init !== undefined) pc.assignFields(this, init, Wire.schema);
    }
  }
  Wire.schema = pc.messageSchemaV1([
    ["first", 0, false, pc.Kind.None, pc.Kind.I32],
    ["middle", 13, false, pc.Kind.None, pc.Kind.I32],
    ["target", 38, false, pc.Kind.None, pc.Kind.I32],
  ]);

  class TargetOnly extends pc.Message {
    target = 0;

    static schema;
  }
  TargetOnly.schema = pc.messageSchemaV1([
    ["target", 38, false, pc.Kind.None, pc.Kind.I32],
  ]);

  const encoded = new Wire({ first: 111, middle: 222, target: 333 })
    .serialize();
  const decoded = TargetOnly.deserialize(encoded);
  assert.equal(decoded.target, 333);
  assert.equal(decoded.hasField("target"), true);
});

test("validates the spans of unknown message fields", () => {
  class Empty extends pc.Message {
    static schema = pc.messageSchemaV1([]);
  }

  const malformed = new Uint32Array([3 << 10]);
  assert.throws(() => Empty.deserialize(malformed), RangeError);
});

test("uses fatal UTF-8 decoding", () => {
  const damaged = new Uint8Array(fixture);
  const needle = new TextEncoder().encode("Hello World!");
  let found = -1;
  for (let offset = 0; offset <= damaged.byteLength - needle.byteLength; offset += 1) {
    if (needle.every((byte, index) => damaged[offset + index] === byte)) {
      found = offset;
      break;
    }
  }
  assert.notEqual(found, -1);
  damaged[found] = 0xff;
  assert.throws(() => Main.deserialize(damaged), TypeError);
});
