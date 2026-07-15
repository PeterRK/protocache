import {
  Kind,
  arraySchemaV1,
  compress,
  decompress,
  deserialize,
  init,
  serialize,
} from "protocache";

export async function runBundledCodec(wasm) {
  await init({ wasm });
  const schema = arraySchemaV1(Kind.String);
  const input = ["bundle", "世界", ""];
  const encoded = serialize(schema, input);
  const decoded = deserialize(schema, encoded);
  const unpacked = decompress(compress(encoded));
  return {
    decoded,
    compressionRoundTrip: indexedEqual(unpacked, encoded),
  };
}

function indexedEqual(left, right) {
  return left.length === right.length &&
    left.every((value, index) => value === right[index]);
}
