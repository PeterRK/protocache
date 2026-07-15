import {
  Kind,
  arraySchemaV1,
  compress,
  decompress,
  deserialize,
  init,
  serialize,
} from "/dist/index.js";
import { loadTest } from "/.test-dist/test/generated/test.pc.js";

const wasmUrl = "/dist/protocache.wasm";

try {
  await init({ wasmUrl });
  const schema = arraySchemaV1(Kind.I32);
  const encoded = serialize(schema, [-1, 0, 7]);
  expect(indexedEqual(deserialize(schema, encoded), [-1, 0, 7]), "browser codec");
  expect(indexedEqual(decompress(compress(encoded)), encoded), "browser compression");

  const generated = await loadTest({ wasmUrl });
  const generatedValue = new generated.Main({ i32: 19, str: "browser" });
  const generatedRoundTrip = generated.Main.deserialize(generatedValue.serialize());
  expect(generatedRoundTrip.i32 === 19, "generated loader integer");
  expect(generatedRoundTrip.str === "browser", "generated loader string");

  const worker = new Worker("/test/platform/browser-worker.mjs", { type: "module" });
  const workerResult = await new Promise((resolve, reject) => {
    const timeout = setTimeout(() => reject(new Error("module Worker timed out")), 15_000);
    worker.onerror = (event) => {
      clearTimeout(timeout);
      reject(new Error(event.message));
    };
    worker.onmessage = (event) => {
      clearTimeout(timeout);
      resolve(event.data);
    };
    worker.postMessage({ wasmUrl });
  });
  worker.terminate();
  expect(workerResult.ok === true, workerResult.error ?? "module Worker failed");
  expect(
    indexedEqual(workerResult.values, ["0", "42", "18446744073709551615"]),
    "module Worker codec",
  );

  await report({ ok: true });
} catch (error) {
  await report({
    ok: false,
    error: error instanceof Error ? error.stack ?? error.message : String(error),
  });
}

function indexedEqual(left, right) {
  return left.length === right.length &&
    left.every((value, index) => value === right[index]);
}

function expect(condition, context) {
  if (!condition) throw new Error(`Failed browser integration check: ${context}`);
}

async function report(result) {
  await fetch("/__result", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(result),
  });
}
