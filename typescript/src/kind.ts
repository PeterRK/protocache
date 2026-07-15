export const Kind = Object.freeze({
  None: 0,
  Message: 1,
  Bytes: 2,
  String: 3,
  F64: 4,
  F32: 5,
  U64: 6,
  U32: 7,
  I64: 8,
  I32: 9,
  Bool: 10,
  Enum: 11,
  Array: 20,
  Map: 21,
} as const);

export type Kind = (typeof Kind)[keyof typeof Kind];

export type ValueKind = Exclude<Kind, typeof Kind.None>;

export type ScalarKind =
  | typeof Kind.Bytes
  | typeof Kind.String
  | typeof Kind.F64
  | typeof Kind.F32
  | typeof Kind.U64
  | typeof Kind.U32
  | typeof Kind.I64
  | typeof Kind.I32
  | typeof Kind.Bool
  | typeof Kind.Enum;

export type ComplexKind =
  | typeof Kind.Message
  | typeof Kind.Array
  | typeof Kind.Map;

export type KeyKind =
  | typeof Kind.String
  | typeof Kind.U64
  | typeof Kind.U32
  | typeof Kind.I64
  | typeof Kind.I32;

const VALUE_KINDS: ReadonlySet<number> = new Set([
  Kind.Message,
  Kind.Bytes,
  Kind.String,
  Kind.F64,
  Kind.F32,
  Kind.U64,
  Kind.U32,
  Kind.I64,
  Kind.I32,
  Kind.Bool,
  Kind.Enum,
  Kind.Array,
  Kind.Map,
]);

const KEY_KINDS: ReadonlySet<number> = new Set([
  Kind.String,
  Kind.U64,
  Kind.U32,
  Kind.I64,
  Kind.I32,
]);

export function isValueKind(value: unknown): value is ValueKind {
  return typeof value === "number" && VALUE_KINDS.has(value);
}

export function isKeyKind(value: unknown): value is KeyKind {
  return typeof value === "number" && KEY_KINDS.has(value);
}

export function isComplexKind(value: ValueKind): value is ComplexKind {
  return value === Kind.Message || value === Kind.Array || value === Kind.Map;
}
