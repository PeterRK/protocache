import { parentPort } from "node:worker_threads";
import { readFile } from "node:fs/promises";

import {
  Kind,
  arraySchemaV1,
  compress,
  decompress,
  deserialize,
  init,
  serialize,
} from "../../dist/index.js";

if (parentPort === null) {
  throw new Error("node-worker.mjs must run in a worker thread");
}

try {
  const wasm = await readFile(new URL("../../dist/protocache.wasm", import.meta.url));
  await init({ wasm });
  const schema = arraySchemaV1(Kind.I32);
  const encoded = serialize(schema, [-7, 0, 42]);
  const values = deserialize(schema, encoded);
  const unpacked = decompress(compress(encoded));
  parentPort.postMessage({
    ok: true,
    values,
    compressionRoundTrip: indexedEqual(unpacked, encoded),
  });
} catch (error) {
  parentPort.postMessage({
    ok: false,
    error: error instanceof Error ? error.stack ?? error.message : String(error),
  });
}

function indexedEqual(left, right) {
  return left.length === right.length &&
    left.every((value, index) => value === right[index]);
}
