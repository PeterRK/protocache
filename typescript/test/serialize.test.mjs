import assert from "node:assert/strict";
import test from "node:test";

import { testApi } from "./init-wasm.mjs";
import * as pc from "../.test-dist/src/index.js";

const {
  CyclicA,
  CyclicB,
  Main,
  Mode,
  Small,
  Vec2DSchema,
} = testApi;

test("serializes non-Map fields and materializes them again", () => {
  const source = new Main({
    i32: -999,
    u32: 0xffff_ffff,
    i64: -9_876_543_210n,
    u64: 18_446_744_073_709_551_615n,
    flag: true,
    mode: Mode.MODE_C,
    str: "Hello 世界 🌍 — this string is external",
    data: new TextEncoder().encode("bytes with enough data to be external"),
    f32: -2.1,
    f64: 1.25,
    object: new Small({ i32: 88, str: "nested object data" }),
    i32v: [1, -2, 3],
    u64v: [0n, 12345678987654321n],
    strv: ["a", "small", "a string that cannot fit inline"],
    datav: [
      new Uint8Array([1, 2]),
      new Uint8Array([3, 4, 5, 6, 7, 8, 9]),
    ],
    f32v: [1.1, 2.2],
    f64v: [9.9, 8.8],
    flags: [true, false, true, false],
    objectv: [
      new Small({ i32: 1 }),
      new Small({ flag: true }),
      new Small({ str: "long nested string" }),
    ],
    matrix: [
      [1, 2, 3],
      [4, 5, 6],
    ],
    modev: [Mode.MODE_A, Mode.MODE_C],
  });

  const bytes = source.serialize();
  const root = Main.deserialize(bytes);

  assert.equal(bytes.byteLength % 4, 0);
  assert.equal(root.i32, source.i32);
  assert.equal(root.u32, source.u32);
  assert.equal(root.i64, source.i64);
  assert.equal(root.u64, source.u64);
  assert.equal(root.flag, true);
  assert.equal(root.mode, Mode.MODE_C);
  assert.equal(root.str, source.str);
  assert.deepEqual(root.data, source.data);
  assert.equal(Math.fround(root.f32), Math.fround(source.f32));
  assert.equal(root.f64, source.f64);
  assert.deepEqual(
    { i32: root.object.i32, flag: root.object.flag, str: root.object.str },
    { i32: 88, flag: false, str: "nested object data" },
  );
  assert.deepEqual(root.i32v, source.i32v);
  assert.deepEqual(root.u64v, source.u64v);
  assert.deepEqual(root.strv, source.strv);
  assert.deepEqual(root.datav, source.datav);
  assert.deepEqual(
    root.f32v.map(Math.fround),
    source.f32v.map(Math.fround),
  );
  assert.deepEqual(root.f64v, source.f64v);
  assert.deepEqual(root.flags, source.flags);
  assert.deepEqual(
    root.objectv.map(({ i32, flag, str }) => ({ i32, flag, str })),
    source.objectv.map(({ i32, flag, str }) => ({ i32, flag, str })),
  );
  assert.deepEqual(root.matrix, source.matrix);
  assert.deepEqual(root.modev, source.modev);

  assert.equal(root.hasField("i32"), true);
  assert.equal(root.hasField("index"), false);
  assert.equal(root.hasField("arrays"), false);
});

test("serializes UTF-8 and byte-array varint boundaries", () => {
  for (const value of [
    "a",
    "é",
    "世",
    "🌍",
    "aé世🌍",
    "x".repeat(31),
    "x".repeat(32),
    "x".repeat(127),
    "x".repeat(128),
    "x".repeat(129),
    `${"x".repeat(127)}é`,
    "x".repeat(4095),
    "x".repeat(4096),
  ]) {
    assert.equal(Main.deserialize(new Main({ str: value }).serialize()).str, value);
  }

  for (const length of [1, 3, 31, 32, 4095, 4096]) {
    const value = Uint8Array.from(
      { length },
      (_, index) => index & 0xff,
    );
    assert.deepEqual(
      Main.deserialize(new Main({ data: value }).serialize()).data,
      value,
    );
  }
});

test("generic serialize supports standalone scalar and nested arrays", () => {
  const row = pc.arraySchemaV1(pc.Kind.F32);

  const rowBytes = pc.serialize(row, [1, 2.5, -3]);
  assert.deepEqual(pc.deserialize(row, rowBytes), [1, 2.5, -3]);

  const matrix = [
    [1, 2, 3],
    [],
    [4, 5, 6],
  ];
  assert.deepEqual(
    pc.deserialize(Vec2DSchema, pc.serialize(Vec2DSchema, matrix)),
    matrix,
  );
});

test("canonicalizes defaults to absent fields", () => {
  const root = Main.deserialize(new Main().serialize());

  assert.equal(root.hasField("i32"), false);
  assert.equal(root.hasField("str"), false);
  assert.equal(root.hasField("object"), false);
  assert.equal(root.hasField("matrix"), false);
  assert.deepEqual(root.i32v, []);
  assert.equal(root.index.size, 0);
});

test("writes section width tables and offsets as two uint32 words", () => {
  class Sectioned extends pc.Message {
    f0 = 0;
    f1 = 0n;
    f12 = 0;
    f27 = 0n;
    f28 = 0n;
    f36 = 0;
    f37 = 0;
    f53 = 0n;
    f61 = 0;
  }

  Sectioned.schema = pc.messageSchemaV1([
    ["f0", 0, false, pc.Kind.None, pc.Kind.I32],
    ["f1", 1, false, pc.Kind.None, pc.Kind.I64],
    ["f12", 12, false, pc.Kind.None, pc.Kind.I32],
    ["f27", 27, false, pc.Kind.None, pc.Kind.I64],
    ["f28", 28, false, pc.Kind.None, pc.Kind.U64],
    ["f36", 36, false, pc.Kind.None, pc.Kind.I32],
    ["f37", 37, false, pc.Kind.None, pc.Kind.I32],
    ["f53", 53, false, pc.Kind.None, pc.Kind.I64],
    ["f61", 61, false, pc.Kind.None, pc.Kind.I32],
  ]);

  const source = new Sectioned();
  source.f0 = 1;
  source.f1 = 2n;
  source.f12 = 3;
  source.f27 = 4n;
  source.f28 = 5n;
  source.f36 = 6;
  source.f37 = 7;
  source.f53 = 8n;
  source.f61 = 9;

  const bytes = source.serialize();
  const words = new Uint32Array(
    bytes.buffer,
    bytes.byteOffset,
    bytes.byteLength / 4,
  );
  assert.deepEqual(
    Array.from(words.subarray(0, 5)),
    [0x0000_0902, 0x8000_0001, 0x000d_0002, 0x0000_0001, 0x0025_0002],
  );

  const root = Sectioned.deserialize(bytes);
  assert.deepEqual(
    [root.f0, root.f1, root.f12, root.f27, root.f28, root.f36,
      root.f37, root.f53, root.f61],
    [1, 2n, 3, 4n, 5n, 6, 7, 8n, 9],
  );
});

test("rejects invalid JS values, isolated surrogates, and cycles", () => {
  assert.throws(
    () => new Main({ i32: 0x8000_0000 }).serialize(),
    /integer/,
  );
  assert.throws(
    () => new Main({ u64: -1n }).serialize(),
    /out of range/,
  );
  assert.throws(
    () => new Main({ flag: "yes" }).serialize(),
    /boolean/,
  );
  assert.throws(
    () => new Main({ str: "\ud800" }).serialize(),
    /isolated UTF-16 surrogate/,
  );
  assert.throws(
    () => new Main({ object: new Main() }).serialize(),
    /match its schema type exactly/,
  );

  const cyclicA = new CyclicA({ value: 1 });
  const cyclicB = new CyclicB({ value: 2, cyclic: cyclicA });
  cyclicA.cyclic = cyclicB;
  assert.throws(() => cyclicA.serialize(), /cyclic object graph/);
  cyclicA.cyclic = undefined;
  assert.equal(cyclicA.serialize().byteLength > 0, true);

  const deep = [new CyclicA({ value: 0 })];
  for (let index = 1; index <= 20; index += 1) {
    const next = index % 2 === 0
      ? new CyclicA({ value: index })
      : new CyclicB({ value: index });
    deep[index - 1].cyclic = next;
    deep.push(next);
  }
  assert.equal(deep[0].serialize().byteLength > 0, true);
  deep.at(-1).cyclic = deep[3];
  assert.throws(() => deep[0].serialize(), /cyclic object graph/);
  deep.at(-1).cyclic = undefined;
  assert.equal(deep[0].serialize().byteLength > 0, true);
});
