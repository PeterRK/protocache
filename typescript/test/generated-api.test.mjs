import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import { loadTest } from "../.test-dist/test/generated/test.pc.js";

const wasm = new Uint8Array(
  readFileSync(new URL("../dist/protocache.wasm", import.meta.url)),
);
const fixture = new Uint8Array(
  readFileSync(new URL("../.test-fixtures/test.pc", import.meta.url)),
);

test("loadTest is the initialized public path to generated values", async () => {
  const loading = loadTest({ wasm });

  assert.equal("Main" in loading, false);
  assert.equal(loadTest({ wasm: new Uint8Array() }), loading);

  const generated = await loading;
  const root = generated.Main.deserialize(fixture);
  const created = new generated.Main({ i32: 7 });

  assert.equal(root.i32, -999);
  assert.equal(created.i32, 7);
});
