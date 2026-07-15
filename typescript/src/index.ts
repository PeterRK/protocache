export {
  Kind,
  isComplexKind,
  isKeyKind,
  isValueKind,
  type ComplexKind,
  type KeyKind,
  type Kind as KindValue,
  type ScalarKind,
  type ValueKind,
} from "./kind.js";

export {
  arraySchemaV1,
  mapSchemaV1,
  messageSchemaV1,
  unknownEnum,
  type ArraySchema,
  type DataFieldName,
  type EnumValue,
  type FieldSchema,
  type FieldSchemaFor,
  type MapSchema,
  type MessageConstructor,
  type MessageFieldsV1,
  type MessageInit,
  type MessageSchema,
  type RuntimeType,
  type UnknownEnum,
} from "./schema.js";

export { deserialize } from "./deserialize.js";
export { serialize } from "./serialize.js";
export {
  compress,
  decompress,
  ensureInitialized,
  init,
  ProtoCacheWasmError,
  ProtoCacheWasmNotInitializedError,
  type InitOptions,
  type WasmSource,
} from "./wasm-runtime.js";
export type { BinaryInput } from "./binary.js";
export { Message, assignFields } from "./model.js";
