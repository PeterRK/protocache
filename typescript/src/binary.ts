export type BinaryInput = ArrayBufferLike | ArrayBufferView;

function isShared(buffer: ArrayBufferLike): buffer is SharedArrayBuffer {
  return (
    typeof SharedArrayBuffer !== "undefined" &&
    buffer instanceof SharedArrayBuffer
  );
}

export function inputBytes(input: BinaryInput): Uint8Array {
  let bytes: Uint8Array;
  if (ArrayBuffer.isView(input)) {
    bytes = new Uint8Array(input.buffer, input.byteOffset, input.byteLength);
  } else if (input instanceof ArrayBuffer || isShared(input)) {
    bytes = new Uint8Array(input);
  } else {
    throw new TypeError("ProtoCache input must be an ArrayBuffer or ArrayBufferView");
  }
  return isShared(bytes.buffer) ? new Uint8Array(bytes) : bytes;
}
