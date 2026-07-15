import { compiledMessageFields } from "./compiled-fields.js";
import { Kind, isComplexKind, type KeyKind, type ValueKind } from "./kind.js";
import type { RuntimeType } from "./schema.js";
import {
  commitDecodeSchema,
  requireWasmInitialized,
  reserveDecodeSchema,
} from "./wasm-runtime.js";

const NO_TYPE = 0xffff_ffff;

interface SchemaFieldNode {
  readonly id: number;
  readonly kind: ValueKind;
  readonly child?: SchemaNode;
}

interface CachedSchemaNode {
  wasmHandle?: number;
}

interface MessageSchemaNode extends CachedSchemaNode {
  readonly kind: typeof Kind.Message;
  readonly fields: SchemaFieldNode[];
}

interface ArraySchemaNode extends CachedSchemaNode {
  readonly kind: typeof Kind.Array;
  readonly valueKind: ValueKind;
  child?: SchemaNode;
}

interface MapSchemaNode extends CachedSchemaNode {
  readonly kind: typeof Kind.Map;
  readonly keyKind: KeyKind;
  readonly valueKind: ValueKind;
  child?: SchemaNode;
}

type SchemaNode = MessageSchemaNode | ArraySchemaNode | MapSchemaNode;

const schemaNodes = new WeakMap<object, SchemaNode>();

function containerNode(
  identity: object,
  kind: typeof Kind.Array | typeof Kind.Map,
  keyKind: typeof Kind.None | KeyKind,
  valueKind: ValueKind,
  resolver: (() => RuntimeType) | undefined,
): ArraySchemaNode | MapSchemaNode {
  const cached = schemaNodes.get(identity);
  if (cached !== undefined) {
    return cached as ArraySchemaNode | MapSchemaNode;
  }
  const node: ArraySchemaNode | MapSchemaNode = kind === Kind.Array
    ? { kind, valueKind }
    : { kind, keyKind: keyKind as KeyKind, valueKind };
  schemaNodes.set(identity, node);
  if (isComplexKind(valueKind)) node.child = schemaNode(resolver!());
  return node;
}

function schemaNode(type: RuntimeType): SchemaNode {
  const identity = type as object;
  const cached = schemaNodes.get(identity);
  if (cached !== undefined) return cached;
  if (typeof type !== "function") {
    return type.kind === Kind.Array
      ? containerNode(
          identity, Kind.Array, Kind.None,
          type.valueKind, type.valueType,
        )
      : containerNode(
          identity, Kind.Map, type.keyKind,
          type.valueKind, type.valueType,
        );
  }
  const node: MessageSchemaNode = { kind: Kind.Message, fields: [] };
  schemaNodes.set(identity, node);
  for (const field of compiledMessageFields(type)) {
    let child: SchemaNode | undefined;
    if (field.repeated) {
      child = containerNode(
        field,
        field.keyKind === Kind.None ? Kind.Array : Kind.Map,
        field.keyKind,
        field.valueKind,
        field.resolver,
      );
    } else if (isComplexKind(field.valueKind)) {
      child = schemaNode(field.resolver!());
    }
    node.fields.push({
      id: field.id,
      kind: field.kind,
      ...(child === undefined ? {} : { child }),
    });
  }
  return node;
}

function wordsToBytes(words: readonly number[]): Uint8Array {
  const bytes = new Uint8Array(words.length * 4);
  const view = new DataView(bytes.buffer);
  for (let index = 0; index < words.length; index += 1) {
    view.setUint32(index * 4, words[index] ?? 0, true);
  }
  return bytes;
}

export function compiledSchemaHandle(type: RuntimeType): number {
  requireWasmInitialized();
  const touched: SchemaNode[] = [];
  const register = (node: SchemaNode): number => {
    if (node.wasmHandle !== undefined) return node.wasmHandle;
    const handle = reserveDecodeSchema(node.kind);
    node.wasmHandle = handle;
    touched.push(node);
    let words: number[];
    if (node.kind === Kind.Message) {
      const fields = node.fields;
      words = [node.kind, fields.length, 4 + fields.length * 3, NO_TYPE];
      for (const field of fields) {
        words.push(
          field.id,
          field.kind,
          field.child === undefined ? NO_TYPE : register(field.child),
        );
      }
    } else if (node.kind === Kind.Array) {
      const child = node.child === undefined ? NO_TYPE : register(node.child);
      words = [node.kind, node.valueKind, 4, child];
    } else {
      const child = node.child === undefined ? NO_TYPE : register(node.child);
      words = [node.kind, node.keyKind | (node.valueKind << 8), 4, child];
    }
    commitDecodeSchema(handle, wordsToBytes(words));
    return handle;
  };
  try {
    return register(schemaNode(type));
  } catch (error) {
    for (const node of touched) {
      delete node.wasmHandle;
    }
    throw error;
  }
}
