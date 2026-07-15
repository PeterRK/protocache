import assert from "node:assert/strict";
import test from "node:test";

import * as pc from "../.test-dist/src/index.js";
import { Main } from "../.test-dist/test/generated/test.pc.internal.js";

test("all codec operations require initialized WASM", () => {
  assert.throws(
    () => new Main().serialize(),
    pc.ProtoCacheWasmNotInitializedError,
  );
  assert.throws(
    () => Main.deserialize(new Uint8Array()),
    pc.ProtoCacheWasmNotInitializedError,
  );
  assert.throws(
    () => pc.compress(new Uint8Array()),
    pc.ProtoCacheWasmNotInitializedError,
  );
  assert.throws(
    () => pc.decompress(new Uint8Array()),
    pc.ProtoCacheWasmNotInitializedError,
  );
});

test("uninitialized codec calls do not resolve schema thunks", () => {
  let resolves = 0;

  class Child extends pc.Message {
    static schema = pc.messageSchemaV1([]);
  }
  class Root extends pc.Message {
    static schema;
  }
  Root.schema = pc.messageSchemaV1([
    ["child", 0, false, pc.Kind.None, pc.Kind.Message, () => {
      resolves += 1;
      return Child;
    }],
  ]);

  assert.throws(
    () => Root.deserialize(Uint32Array.of(0)),
    pc.ProtoCacheWasmNotInitializedError,
  );
  assert.throws(
    () => new Root().serialize(),
    pc.ProtoCacheWasmNotInitializedError,
  );
  assert.equal(resolves, 0);
});
