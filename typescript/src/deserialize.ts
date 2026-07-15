import { inputBytes, type BinaryInput } from "./binary.js";
import { deserializeWithTape } from "./deserialize-tape.js";
import type { Message } from "./model.js";
import type {
  ArraySchema,
  MapSchema,
  MessageConstructor,
  RuntimeType,
} from "./schema.js";

let deserializationActive = false;

export function deserialize<T extends Message>(
  type: MessageConstructor<T>,
  input: BinaryInput,
): T;
export function deserialize<Element>(
  type: ArraySchema<Element>,
  input: BinaryInput,
): Element[];
export function deserialize<Key, Value>(
  type: MapSchema<Key, Value>,
  input: BinaryInput,
): Map<Key, Value>;
export function deserialize(
  type: RuntimeType,
  input: BinaryInput,
): Message | unknown[] | Map<unknown, unknown> {
  const bytes = inputBytes(input);
  if (deserializationActive) {
    throw new TypeError("ProtoCache WASM deserialization is not reentrant");
  }
  deserializationActive = true;
  try {
    return deserializeWithTape(type, bytes);
  } finally {
    deserializationActive = false;
  }
}
