import assert from "node:assert/strict";
import test from "node:test";

import * as pc from "../.test-dist/src/index.js";
import {
  ArrMap_ArraySchema,
  ArrMapSchema,
  CyclicA,
  CyclicB,
  Main,
  Mode,
  Small,
  Vec2D_Vec1DSchema,
} from "../.test-dist/test/generated/test.pc.internal.js";

test("generated constructor applies init after field defaults", () => {
  const index = new Map([["answer", 42]]);
  const root = new Main({ i32: 7, i64: 9n, str: "ready", index });

  assert.equal(root.i32, 7);
  assert.equal(root.i64, 9n);
  assert.equal(root.str, "ready");
  assert.equal(root.index, index);
  assert.deepEqual(root.i32v, []);
  assert.equal(root.hasField("i32"), false);
});

test("mutable defaults are allocated per generated instance", () => {
  const first = new Main();
  const second = new Main();

  first.i32v.push(1);
  first.index.set("one", 1);

  assert.notEqual(first.i32v, second.i32v);
  assert.notEqual(first.index, second.index);
  assert.notEqual(first.data, second.data);
  assert.deepEqual(second.i32v, []);
  assert.equal(second.index.size, 0);
  assert.equal(second.data.byteLength, 0);
});

test("constructor rejects unknown JS fields without polluting objects", () => {
  assert.throws(() => new Main({ missing: 1 }), /Unknown ProtoCache field: missing/);

  const keys = Object.keys(new Small({ i32: 1 }));
  assert.deepEqual(keys, ["i32", "flag", "str"]);
  assert.equal(keys.some((name) => name.startsWith("_")), false);
});

test("schema metadata matches the shared test.proto field layout", () => {
  const summary = Main.schema.fields.map(
    ([name, fieldId, repeated, keyKind, valueKind, valueType]) => [
      name,
      fieldId,
      repeated,
      keyKind,
      valueKind,
      valueType !== undefined,
    ],
  );

  assert.deepEqual(summary, [
    ["i32", 0, false, 0, 9, false],
    ["u32", 1, false, 0, 7, false],
    ["i64", 2, false, 0, 8, false],
    ["u64", 3, false, 0, 6, false],
    ["flag", 4, false, 0, 10, false],
    ["mode", 5, false, 0, 11, false],
    ["str", 6, false, 0, 3, false],
    ["data", 7, false, 0, 2, false],
    ["f32", 8, false, 0, 5, false],
    ["f64", 9, false, 0, 4, false],
    ["object", 10, false, 0, 1, true],
    ["i32v", 11, true, 0, 9, false],
    ["u64v", 12, true, 0, 6, false],
    ["strv", 13, true, 0, 3, false],
    ["datav", 14, true, 0, 2, false],
    ["f32v", 15, true, 0, 5, false],
    ["f64v", 16, true, 0, 4, false],
    ["flags", 17, true, 0, 10, false],
    ["objectv", 18, true, 0, 1, true],
    ["t_u32", 19, false, 0, 7, false],
    ["t_i32", 20, false, 0, 9, false],
    ["t_s32", 21, false, 0, 9, false],
    ["t_u64", 22, false, 0, 6, false],
    ["t_i64", 23, false, 0, 8, false],
    ["t_s64", 24, false, 0, 8, false],
    ["index", 25, true, 3, 9, false],
    ["objects", 26, true, 9, 1, true],
    ["matrix", 27, false, 0, 20, true],
    ["vector", 28, true, 0, 21, true],
    ["arrays", 29, false, 0, 21, true],
    ["modev", 31, true, 0, 11, false],
  ]);
  assert.equal(Object.isFrozen(Main.schema), true);
  assert.equal(Object.isFrozen(Main.schema.fields), true);
  assert.equal(Object.isFrozen(Main.schema.fields[0]), true);
  assert.deepEqual(
    Small.schema.fields.map(([name, fieldId]) => [name, fieldId]),
    [
      ["i32", 0],
      ["flag", 1],
      ["str", 3],
    ],
  );
});

test("complex resolvers stay lazy and recursive types load safely", () => {
  let resolved = false;
  const schema = pc.messageSchemaV1([
    [
      "child",
      0,
      false,
      pc.Kind.None,
      pc.Kind.Message,
      () => {
        resolved = true;
        return CyclicA;
      },
    ],
  ]);

  assert.equal(resolved, false);
  assert.equal(schema.fields[0][5](), CyclicA);
  assert.equal(resolved, true);
  assert.equal(CyclicA.schema.fields[1][5](), CyclicB);
  assert.equal(CyclicB.schema.fields[1][5](), CyclicA);
});

test("alias schemas use native containers and immutable tokens", () => {
  assert.equal(Vec2D_Vec1DSchema.kind, pc.Kind.Array);
  assert.equal(ArrMapSchema.kind, pc.Kind.Map);
  assert.equal(ArrMapSchema.valueType(), ArrMap_ArraySchema);
  assert.equal(Object.isFrozen(Vec2D_Vec1DSchema), true);
  assert.equal(Object.isFrozen(ArrMapSchema), true);
});

test("enum helper preserves future int32 values and checks range", () => {
  assert.equal(Mode.MODE_C, 2);
  assert.equal(pc.unknownEnum(123), 123);
  assert.throws(() => pc.unknownEnum(0x8000_0000), RangeError);
  assert.throws(() => pc.unknownEnum(1.5), RangeError);
});

test("metadata validation catches malformed runtime JS", () => {
  assert.throws(
    () =>
      pc.messageSchemaV1([
        ["child", 0, false, pc.Kind.None, pc.Kind.Message],
      ]),
    /requires a lazy complex-type resolver/,
  );
  assert.throws(
    () =>
      pc.messageSchemaV1([
        ["one", 0, false, pc.Kind.None, pc.Kind.I32],
        ["two", 0, false, pc.Kind.None, pc.Kind.I32],
      ]),
    /duplicates a name or field id/,
  );
  assert.throws(
    () =>
      pc.messageSchemaV1([
        ["tooFar", 6387, false, pc.Kind.None, pc.Kind.I32],
      ]),
    /invalid field id/,
  );
  assert.throws(
    () => pc.mapSchemaV1(pc.Kind.Bool, pc.Kind.I32),
    /invalid key kind/,
  );
});
