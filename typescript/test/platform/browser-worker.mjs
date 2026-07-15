import {
  Kind,
  arraySchemaV1,
  deserialize,
  init,
  serialize,
} from "/dist/index.js";

self.onmessage = async (event) => {
  try {
    await init({ wasmUrl: event.data.wasmUrl });
    const schema = arraySchemaV1(Kind.U64);
    const encoded = serialize(schema, [0n, 42n, (1n << 64n) - 1n]);
    const values = deserialize(schema, encoded);
    self.postMessage({ ok: true, values: values.map(String) });
  } catch (error) {
    self.postMessage({
      ok: false,
      error: error instanceof Error ? error.stack ?? error.message : String(error),
    });
  }
};
