type PresenceBits = number | Uint32Array;

const presence = Symbol("ProtoCache.presence");
const indexesByPrototype = new WeakMap<object, ReadonlyMap<string, number>>();

type MessageWithPresence = object & {
  [presence]?: PresenceBits;
};

export function registerPresentFields(
  prototype: object,
  indexes: ReadonlyMap<string, number>,
): void {
  if (!indexesByPrototype.has(prototype)) {
    indexesByPrototype.set(prototype, indexes);
  }
}

export function hasPresentField(message: object, field: string): boolean {
  const bits = (message as MessageWithPresence)[presence];
  if (bits === undefined) return false;
  const indexes = indexesByPrototype.get(Object.getPrototypeOf(message));
  if (indexes === undefined) return false;
  const index = indexes.get(field);
  if (index === undefined) return false;
  const word = typeof bits === "number" ? bits : bits[index >>> 5] ?? 0;
  return (
    (word & (1 << (index & 31))) !== 0
  );
}

export function setPresentFields(
  message: object,
  bits: PresenceBits,
): void {
  (message as MessageWithPresence)[presence] = bits;
}
