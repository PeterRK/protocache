import {
  Kind,
  isComplexKind,
  isKeyKind,
  isValueKind,
  type ComplexKind,
  type KeyKind,
  type ValueKind,
} from "./kind.js";
import type { Message } from "./model.js";

declare const enumBrand: unique symbol;
declare const schemaValueBrand: unique symbol;

export type UnknownEnum = number & {
  readonly [enumBrand]: "unknown";
};

export type EnumValue<Known extends number> = Known | UnknownEnum;

export function unknownEnum(value: number): UnknownEnum {
  if (!Number.isInteger(value) || value < -0x8000_0000 || value > 0x7fff_ffff) {
    throw new RangeError("ProtoCache enum value must be a signed 32-bit integer");
  }
  return value as UnknownEnum;
}

type AnyMethod = (...args: never[]) => unknown;

export type DataFieldName<T> = Extract<
  {
    [Name in keyof T]-?: T[Name] extends AnyMethod ? never : Name;
  }[keyof T],
  string
>;

export type MessageInit<T extends object> = Partial<
  Pick<T, DataFieldName<T>>
>;

export interface MessageConstructor<T extends Message = Message> {
  new (init?: MessageInit<T>): T;
  readonly prototype: T;
  schema: MessageSchema<T>;
}

export interface MessageSchema<T extends object = object> {
  readonly version: 1;
  readonly kind: typeof Kind.Message;
  readonly fields: MessageFieldsV1<T>;
  readonly [schemaValueBrand]?: (value: T) => T;
}

export interface ArraySchema<Element = unknown> {
  readonly version: 1;
  readonly kind: typeof Kind.Array;
  readonly valueKind: ValueKind;
  readonly valueType?: () => RuntimeType;
  readonly [schemaValueBrand]?: (value: Element[]) => Element[];
}

export interface MapSchema<Key = unknown, Value = unknown> {
  readonly version: 1;
  readonly kind: typeof Kind.Map;
  readonly keyKind: KeyKind;
  readonly valueKind: ValueKind;
  readonly valueType?: () => RuntimeType;
  readonly [schemaValueBrand]?: (value: Map<Key, Value>) => Map<Key, Value>;
}

export type RuntimeType =
  | MessageConstructor<any>
  | ArraySchema<any>
  | MapSchema<any, any>;

export function isMessageConstructor(
  value: unknown,
): value is MessageConstructor<Message> {
  if (typeof value !== "function") return false;
  const schema = (value as { schema?: unknown }).schema;
  return (
    typeof schema === "object" && schema !== null &&
    (schema as { kind?: unknown }).kind === Kind.Message
  );
}

export function resolveRuntimeType(
  resolver: (() => RuntimeType) | undefined,
  expectedKind: ComplexKind,
): RuntimeType {
  if (resolver === undefined) {
    throw new TypeError("ProtoCache complex metadata is missing a resolver");
  }
  const candidate = resolver();
  const valid = expectedKind === Kind.Message
    ? isMessageConstructor(candidate)
    : typeof candidate === "object" && candidate !== null &&
      candidate.kind === expectedKind;
  if (!valid) {
    throw new TypeError(
      `ProtoCache resolver did not return a kind ${expectedKind} runtime type`,
    );
  }
  return candidate;
}

export function cachedRuntimeTypeResolver(
  resolver: (() => RuntimeType) | undefined,
  expectedKind: ComplexKind,
): () => RuntimeType {
  let resolved: RuntimeType | undefined;
  return () => {
    resolved ??= resolveRuntimeType(resolver, expectedKind);
    return resolved;
  };
}

type IsBroadNumber<Value> = [Value] extends [number]
  ? [number] extends [Value]
    ? true
    : false
  : false;

type IsEnumNumber<Value> = [Value] extends [number]
  ? [number] extends [Value]
    ? false
    : true
  : false;

type ValueMatchesKind<Value, ValueType extends ValueKind> =
  ValueType extends typeof Kind.Message
    ? NonNullable<Value> extends Message
      ? true
      : false
    : ValueType extends typeof Kind.Array
      ? Value extends readonly unknown[]
        ? true
        : false
      : ValueType extends typeof Kind.Map
        ? Value extends ReadonlyMap<unknown, unknown>
          ? true
          : false
        : ValueType extends typeof Kind.Bytes
          ? Value extends Uint8Array
            ? true
            : false
          : ValueType extends typeof Kind.String
            ? Value extends string
              ? true
              : false
            : ValueType extends typeof Kind.Bool
              ? Value extends boolean
                ? true
                : false
              : ValueType extends typeof Kind.U64 | typeof Kind.I64
                ? Value extends bigint
                  ? true
                  : false
                : ValueType extends typeof Kind.Enum
                  ? IsEnumNumber<Value>
                  : ValueType extends
                        | typeof Kind.F64
                        | typeof Kind.F32
                        | typeof Kind.U32
                        | typeof Kind.I32
                    ? IsBroadNumber<Value>
                    : false;

type KeyMatchesKind<Key, KeyType extends KeyKind> =
  KeyType extends typeof Kind.String
    ? Key extends string
      ? true
      : false
    : KeyType extends typeof Kind.Bool
      ? Key extends boolean
        ? true
        : false
      : KeyType extends typeof Kind.U64 | typeof Kind.I64
        ? Key extends bigint
          ? true
          : false
        : KeyType extends typeof Kind.U32 | typeof Kind.I32
          ? IsBroadNumber<Key>
          : false;

type ResolverFor<Value, ValueType extends ComplexKind> =
  ValueType extends typeof Kind.Message
    ? () => MessageConstructor<Extract<NonNullable<Value>, Message>>
    : ValueType extends typeof Kind.Array
      ? NonNullable<Value> extends readonly (infer Element)[]
        ? () => ArraySchema<Element>
        : never
      : NonNullable<Value> extends ReadonlyMap<infer Key, infer Item>
        ? () => MapSchema<Key, Item>
        : never;

type FieldTail<Value, ValueType extends ValueKind> =
  ValueType extends ComplexKind
    ? readonly [valueType: ResolverFor<Value, ValueType>]
    : readonly [];

type SingularField<
  Name extends string,
  Value,
  ValueType extends ValueKind,
> = ValueMatchesKind<Value, ValueType> extends true
  ? readonly [
      name: Name,
      fieldId: number,
      repeated: false,
      keyKind: typeof Kind.None,
      valueKind: ValueType,
      ...tail: FieldTail<Value, ValueType>,
    ]
  : never;

type RepeatedField<
  Name extends string,
  Value,
  ValueType extends ValueKind,
> = Value extends readonly (infer Element)[]
  ? ValueMatchesKind<Element, ValueType> extends true
    ? readonly [
        name: Name,
        fieldId: number,
        repeated: true,
        keyKind: typeof Kind.None,
        valueKind: ValueType,
        ...tail: FieldTail<Element, ValueType>,
      ]
    : never
  : never;

type MapFieldForValue<
  Name extends string,
  Key,
  Value,
  KeyType extends KeyKind,
  ValueType extends ValueKind,
> = KeyMatchesKind<Key, KeyType> extends true
  ? ValueMatchesKind<Value, ValueType> extends true
    ? readonly [
        name: Name,
        fieldId: number,
        repeated: true,
        keyKind: KeyType,
        valueKind: ValueType,
        ...tail: FieldTail<Value, ValueType>,
      ]
    : never
  : never;

type MapField<
  Name extends string,
  Value,
  KeyType extends KeyKind,
  ValueType extends ValueKind,
> = Value extends ReadonlyMap<infer Key, infer Item>
  ? MapFieldForValue<Name, Key, Item, KeyType, ValueType>
  : never;

type SingularFields<Name extends string, Value> = {
  [ValueType in ValueKind]: SingularField<Name, Value, ValueType>;
}[ValueKind];

type RepeatedFields<Name extends string, Value> = {
  [ValueType in ValueKind]: RepeatedField<Name, Value, ValueType>;
}[ValueKind];

type MapFields<Name extends string, Value> = {
  [KeyType in KeyKind]: {
    [ValueType in ValueKind]: MapField<Name, Value, KeyType, ValueType>;
  }[ValueKind];
}[KeyKind];

export type FieldSchemaFor<T extends object> = {
  [Name in DataFieldName<T>]:
    | SingularFields<Name, T[Name]>
    | RepeatedFields<Name, T[Name]>
    | MapFields<Name, T[Name]>;
}[DataFieldName<T>];

export type MessageFieldsV1<T extends object> = readonly FieldSchemaFor<T>[];

export type FieldSchema = readonly [
  name: string,
  fieldId: number,
  repeated: boolean,
  keyKind: typeof Kind.None | KeyKind,
  valueKind: ValueKind,
  valueType?: () => RuntimeType,
];

type ArraySchemaArgs<Element> = {
  [ValueType in ValueKind]: ValueMatchesKind<Element, ValueType> extends true
    ? readonly [
        valueKind: ValueType,
        ...tail: FieldTail<Element, ValueType>,
      ]
    : never;
}[ValueKind];

type MapSchemaArgs<Key, Value> = {
  [KeyType in KeyKind]: KeyMatchesKind<Key, KeyType> extends true
    ? {
        [ValueType in ValueKind]: ValueMatchesKind<
          Value,
          ValueType
        > extends true
          ? readonly [
              keyKind: KeyType,
              valueKind: ValueType,
              ...tail: FieldTail<Value, ValueType>,
            ]
          : never;
      }[ValueKind]
    : never;
}[KeyKind];

function checkValueType(
  valueKind: unknown,
  valueType: unknown,
  context: string,
): asserts valueKind is ValueKind {
  if (!isValueKind(valueKind)) {
    throw new TypeError(`${context} has an invalid value kind`);
  }
  if (isComplexKind(valueKind)) {
    if (typeof valueType !== "function") {
      throw new TypeError(`${context} requires a lazy complex-type resolver`);
    }
  } else if (valueType !== undefined) {
    throw new TypeError(`${context} must not have a complex-type resolver`);
  }
}

export function messageSchemaV1<T extends object>(
  fields: MessageFieldsV1<T>,
): MessageSchema<T> {
  const names = new Set<string>();
  const ids = new Set<number>();
  const normalized: FieldSchema[] = [];

  for (const candidate of fields as readonly FieldSchema[]) {
    if (!Array.isArray(candidate) || (candidate.length !== 5 && candidate.length !== 6)) {
      throw new TypeError("ProtoCache message field metadata must be a 5- or 6-tuple");
    }
    const [name, fieldId, repeated, keyKind, valueKind, valueType] = candidate;
    if (typeof name !== "string" || name.length === 0) {
      throw new TypeError("ProtoCache field name must be a non-empty string");
    }
    if (!Number.isSafeInteger(fieldId) || fieldId < 0 || fieldId >= 6387) {
      throw new TypeError(`ProtoCache field ${name} has an invalid field id`);
    }
    if (typeof repeated !== "boolean") {
      throw new TypeError(`ProtoCache field ${name} has an invalid repeated flag`);
    }
    if (
      keyKind !== Kind.None &&
      (!repeated || !isKeyKind(keyKind))
    ) {
      throw new TypeError(`ProtoCache field ${name} has an invalid map key kind`);
    }
    checkValueType(valueKind, valueType, `ProtoCache field ${name}`);
    if (names.has(name) || ids.has(fieldId)) {
      throw new TypeError(`ProtoCache field ${name} duplicates a name or field id`);
    }
    names.add(name);
    ids.add(fieldId);
    normalized.push(
      Object.freeze(
        valueType === undefined
          ? [name, fieldId, repeated, keyKind, valueKind]
          : [name, fieldId, repeated, keyKind, valueKind, valueType],
      ) as FieldSchema,
    );
  }

  return Object.freeze({
    version: 1,
    kind: Kind.Message,
    fields: Object.freeze(normalized),
  }) as MessageSchema<T>;
}

export function arraySchemaV1<Element>(
  ...args: ArraySchemaArgs<Element>
): ArraySchema<Element> {
  const [valueKind, valueType] = args as readonly [
    ValueKind,
    (() => RuntimeType) | undefined,
  ];
  checkValueType(valueKind, valueType, "ProtoCache array schema");
  return Object.freeze(
    valueType === undefined
      ? { version: 1, kind: Kind.Array, valueKind }
      : { version: 1, kind: Kind.Array, valueKind, valueType },
  ) as ArraySchema<Element>;
}

export function mapSchemaV1<Key, Value>(
  ...args: MapSchemaArgs<Key, Value>
): MapSchema<Key, Value> {
  const [keyKind, valueKind, valueType] = args as readonly [
    KeyKind,
    ValueKind,
    (() => RuntimeType) | undefined,
  ];
  if (!isKeyKind(keyKind)) {
    throw new TypeError("ProtoCache map schema has an invalid key kind");
  }
  checkValueType(valueKind, valueType, "ProtoCache map schema");
  return Object.freeze(
    valueType === undefined
      ? { version: 1, kind: Kind.Map, keyKind, valueKind }
      : { version: 1, kind: Kind.Map, keyKind, valueKind, valueType },
  ) as MapSchema<Key, Value>;
}
