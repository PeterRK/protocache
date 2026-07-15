import assert from "node:assert/strict";
import { once } from "node:events";
import { Worker } from "node:worker_threads";

const worker = new Worker(
  new URL("../test/platform/node-worker.mjs", import.meta.url),
  { type: "module" },
);
const message = Promise.race([
  once(worker, "message").then(([value]) => value),
  once(worker, "error").then(([error]) => Promise.reject(error)),
]);

try {
  const result = await message;
  assert.equal(result.ok, true, result.error);
  assert.deepEqual(result.values, [-7, 0, 42]);
  assert.equal(result.compressionRoundTrip, true);
  console.log("Node module Worker integration test: OK");
} finally {
  await worker.terminate();
}
