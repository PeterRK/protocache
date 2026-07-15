import assert from "node:assert/strict";
import { mkdir, readFile, rm } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { build } from "esbuild";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const outputDirectory = resolve(root, ".test-bundle");
const output = resolve(outputDirectory, "bundle.mjs");

await rm(outputDirectory, { recursive: true, force: true });
await mkdir(outputDirectory, { recursive: true });
await build({
  entryPoints: [resolve(root, "test/platform/bundler-entry.mjs")],
  outfile: output,
  bundle: true,
  format: "esm",
  platform: "browser",
  target: "es2020",
  conditions: ["browser", "import"],
  logLevel: "silent",
});

const bundled = await import(pathToFileURL(output).href);
const wasm = await readFile(resolve(root, "dist/protocache.wasm"));
const result = await bundled.runBundledCodec(wasm);
assert.deepEqual(result.decoded, ["bundle", "世界", ""]);
assert.equal(result.compressionRoundTrip, true);
console.log("esbuild browser bundle integration test: OK");
