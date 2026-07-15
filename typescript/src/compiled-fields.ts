import { Kind, isComplexKind, type KeyKind, type ValueKind } from "./kind.js";
import type { Message } from "./model.js";
import {
  cachedRuntimeTypeResolver,
  type FieldSchema,
  type MessageConstructor,
  type RuntimeType,
} from "./schema.js";
import { registerPresentFields } from "./state.js";

export interface CompiledFieldMetadata {
  readonly name: string;
  readonly id: number;
  readonly repeated: boolean;
  readonly keyKind: typeof Kind.None | KeyKind;
  readonly valueKind: ValueKind;
  readonly kind: ValueKind;
  readonly resolver?: () => RuntimeType;
}

const messages = new WeakMap<object, readonly CompiledFieldMetadata[]>();

export function compiledMessageFields<T extends Message>(
  type: MessageConstructor<T>,
): readonly CompiledFieldMetadata[] {
  const cached = messages.get(type);
  if (cached !== undefined) return cached;
  const fields: CompiledFieldMetadata[] = [];
  const presenceIndexes = new Map<string, number>();
  for (const field of type.schema.fields as readonly FieldSchema[]) {
    const [name, id, repeated, keyKind, valueKind, resolver] = field;
    presenceIndexes.set(name, fields.length);
    fields.push({
      name,
      id,
      repeated,
      keyKind,
      valueKind,
      kind: repeated
        ? keyKind === Kind.None ? Kind.Array : Kind.Map
        : valueKind,
      ...(isComplexKind(valueKind)
        ? { resolver: cachedRuntimeTypeResolver(resolver, valueKind) }
        : {}),
    });
  }
  registerPresentFields(type.prototype, presenceIndexes);
  messages.set(type, fields);
  return fields;
}

export function defaultFactoryForField(
  field: CompiledFieldMetadata,
): () => unknown {
  if (field.repeated) {
    return field.keyKind === Kind.None ? () => [] : () => new Map();
  }
  switch (field.valueKind) {
    case Kind.Message:
      return () => undefined;
    case Kind.Bytes:
      return () => new Uint8Array();
    case Kind.String:
      return () => "";
    case Kind.U64:
    case Kind.I64:
      return () => 0n;
    case Kind.Bool:
      return () => false;
    case Kind.Array:
      return () => [];
    case Kind.Map:
      return () => new Map();
    default:
      return () => 0;
  }
}
