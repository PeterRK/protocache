import { readFileSync } from "node:fs";

import { loadTest } from "../.test-dist/test/generated/test.pc.js";

export const testApi = await loadTest({
  wasm: new Uint8Array(
    readFileSync(new URL("../dist/protocache.wasm", import.meta.url)),
  ),
});
