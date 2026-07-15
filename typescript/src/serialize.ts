import type { Message } from "./model.js";
import type {
  ArraySchema,
  MapSchema,
  MessageConstructor,
  RuntimeType,
} from "./schema.js";
import { serializeWithArena } from "./serialize-arena.js";

export function serialize<T extends Message>(
  type: MessageConstructor<T>,
  value: T,
): Uint8Array;
export function serialize<Element>(
  type: ArraySchema<Element>,
  value: Element[],
): Uint8Array;
export function serialize<Key, Value>(
  type: MapSchema<Key, Value>,
  value: Map<Key, Value>,
): Uint8Array;
export function serialize(type: RuntimeType, value: unknown): Uint8Array {
  return serializeWithArena(type, value);
}
