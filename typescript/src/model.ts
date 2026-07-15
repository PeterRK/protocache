import type {
  BinaryInput,
} from "./binary.js";
import { deserialize } from "./deserialize.js";
import { serialize } from "./serialize.js";
import type {
  DataFieldName,
  MessageConstructor,
  MessageInit,
  MessageSchema,
} from "./schema.js";
import { hasPresentField } from "./state.js";

export class Message {
  static deserialize<T extends Message>(
    this: MessageConstructor<T>,
    input: BinaryInput,
  ): T {
    return deserialize(this, input);
  }

  serialize(): Uint8Array {
    return serialize(
      this.constructor as MessageConstructor<this>,
      this,
    );
  }

  hasField<Name extends DataFieldName<this>>(field: Name): boolean {
    return hasPresentField(this, field);
  }
}

const fieldNames = new WeakMap<object, ReadonlySet<string>>();

export function assignFields<T extends Message>(
  target: T,
  init: MessageInit<T>,
  schema: MessageSchema<T>,
): T {
  let known = fieldNames.get(schema);
  if (known === undefined) {
    known = new Set(schema.fields.map((field) => field[0]));
    fieldNames.set(schema, known);
  }
  for (const name of Object.keys(init)) {
    if (!known.has(name)) {
      throw new TypeError(`Unknown ProtoCache field: ${name}`);
    }
    (target as Record<string, unknown>)[name] = (
      init as Record<string, unknown>
    )[name];
  }
  return target;
}

export type { MessageConstructor, MessageInit };
