import {
  compiledMessageFields,
  defaultFactoryForField,
} from "./compiled-fields.js";
import { compiledSchemaHandle } from "./compiled-schema.js";
import { Kind, isComplexKind, type KeyKind, type ValueKind } from "./kind.js";
import type { Message } from "./model.js";
import {
  isMessageConstructor,
  resolveRuntimeType,
  type RuntimeType,
} from "./schema.js";
import { setPresentFields } from "./state.js";
import { buildDecodeTape } from "./wasm-runtime.js";

const NO_TYPE = 0xffff_ffff;
const textDecoder = new TextDecoder("utf-8", { fatal: true });
const MAX_DIRECT_ASCII_LENGTH = 128;

interface TapeField {
  readonly name: string;
  readonly kind: ValueKind;
  readonly child: number;
  readonly createDefault: () => unknown;
}

interface MessageTapeType {
  readonly kind: typeof Kind.Message;
  readonly prototype: Message;
  readonly fields: readonly TapeField[];
  readonly presenceWordCount: number;
}

interface ArrayTapeType {
  readonly kind: typeof Kind.Array;
  readonly valueKind: ValueKind;
  readonly child: number;
}

interface MapTapeType {
  readonly kind: typeof Kind.Map;
  readonly keyKind: KeyKind;
  readonly valueKind: ValueKind;
  readonly child: number;
}

type TapeType = MessageTapeType | ArrayTapeType | MapTapeType;

interface DecodePlan {
  readonly root: number;
  readonly types: readonly TapeType[];
}

interface MaterializeContext {
  readonly input: Uint8Array;
  readonly view: DataView;
  readonly tape: Uint32Array;
  readonly plan: DecodePlan;
  cursor: number;
}

const plans = new WeakMap<object, DecodePlan>();

function decodeString(
  input: Uint8Array,
  offset: number,
  length: number,
): string {
  if (length <= MAX_DIRECT_ASCII_LENGTH) {
    let out = "";
    const limit = offset + length;
    for (let index = offset; index < limit; index += 1) {
      const byte = input[index] ?? 0;
      if ((byte & 0x80) !== 0) {
        return textDecoder.decode(input.subarray(offset, limit));
      }
      out += String.fromCharCode(byte);
    }
    return out;
  }
  return textDecoder.decode(input.subarray(offset, offset + length));
}

function compilePlan(rootType: RuntimeType): DecodePlan {
  const cached = plans.get(rootType as object);
  if (cached !== undefined) return cached;

  const types: TapeType[] = [];
  const typeIds = new Map<object, number>();

  const addContainer = (
    kind: typeof Kind.Array | typeof Kind.Map,
    keyKind: typeof Kind.None | KeyKind,
    valueKind: ValueKind,
    resolver: (() => RuntimeType) | undefined,
    identity: object,
  ): number => {
    const existing = typeIds.get(identity);
    if (existing !== undefined) return existing;
    const id = types.length;
    typeIds.set(identity, id);
    types.push(undefined as unknown as TapeType);
    const child = isComplexKind(valueKind)
      ? addRuntimeType(resolveRuntimeType(resolver, valueKind))
      : NO_TYPE;
    types[id] = kind === Kind.Array
      ? { kind, valueKind, child }
      : { kind, keyKind: keyKind as KeyKind, valueKind, child };
    return id;
  };

  const addRuntimeType = (type: RuntimeType): number => {
    const identity = type as object;
    const existing = typeIds.get(identity);
    if (existing !== undefined) return existing;
    if (typeof type !== "function") {
      return type.kind === Kind.Array
        ? addContainer(
            Kind.Array,
            Kind.None,
            type.valueKind,
            type.valueType,
            identity,
          )
        : addContainer(
            Kind.Map,
            type.keyKind,
            type.valueKind,
            type.valueType,
            identity,
          );
    }
    if (!isMessageConstructor(type)) {
      throw new TypeError("Invalid ProtoCache message runtime type");
    }
    const id = types.length;
    typeIds.set(identity, id);
    types.push(undefined as unknown as TapeType);
    const sharedFields = compiledMessageFields(type);
    const fields: TapeField[] = [];
    for (const field of sharedFields) {
      const { name, repeated, keyKind, valueKind, resolver } = field;
      let kind = field.kind;
      let child = NO_TYPE;
      if (repeated) {
        const containerKind = keyKind === Kind.None ? Kind.Array : Kind.Map;
        kind = containerKind;
        child = addContainer(
          containerKind,
          keyKind,
          valueKind,
          resolver,
          field,
        );
      } else if (isComplexKind(valueKind)) {
        child = addRuntimeType(resolveRuntimeType(resolver, valueKind));
      }
      fields.push({
        name,
        kind,
        child,
        createDefault: defaultFactoryForField(field),
      });
    }
    types[id] = {
      kind: Kind.Message,
      prototype: type.prototype,
      fields,
      presenceWordCount: Math.ceil(fields.length / 32),
    };
    return id;
  };

  const root = addRuntimeType(rootType);
  const plan = { root, types } satisfies DecodePlan;
  plans.set(rootType as object, plan);
  return plan;
}

function scalarAt(
  context: MaterializeContext,
  kind: ValueKind | KeyKind,
  first: number,
  second: number,
): unknown {
  if (kind === Kind.String) {
    return decodeString(context.input, first, second);
  }
  if (kind === Kind.Bytes) {
    return context.input.slice(first, first + second);
  }
  const offset = first * 4;
  switch (kind) {
    case Kind.Bool:
      return context.view.getUint32(offset, true) !== 0;
    case Kind.I32:
    case Kind.Enum:
      return context.view.getInt32(offset, true);
    case Kind.U32:
      return context.view.getUint32(offset, true);
    case Kind.I64:
      return context.view.getBigInt64(offset, true);
    case Kind.U64:
      return context.view.getBigUint64(offset, true);
    case Kind.F32:
      return context.view.getFloat32(offset, true);
    case Kind.F64:
      return context.view.getFloat64(offset, true);
  }
}

function materialize(context: MaterializeContext, expectedType: number): unknown {
  const start = context.cursor;
  // The tape is private output from the ABI-checked native decoder. Its record
  // kind, type and message field count are already constrained by this plan;
  // checking them again at every recursive node only duplicates native work.
  const type = context.plan.types[expectedType]!;

  if (type.kind === Kind.Message) {
    const descriptors = start + 5;
    context.cursor = descriptors + type.fields.length * 2;
    const out = Object.create(type.prototype) as Message;
    let presentWord = 0;
    const presentWords = type.presenceWordCount <= 1
      ? undefined
      : new Uint32Array(type.presenceWordCount);
    for (let presenceIndex = 0; presenceIndex < type.fields.length; presenceIndex += 1) {
      const field = type.fields[presenceIndex]!;
      const descriptor = descriptors + presenceIndex * 2;
      const first = context.tape[descriptor] ?? 0;
      const second = context.tape[descriptor + 1] ?? 0;
      let value: unknown;
      if (first === 0) {
        value = field.createDefault();
      } else {
        value = isComplexKind(field.kind)
          ? materialize(context, field.child)
          : scalarAt(context, field.kind, first, second);
        if (presentWords === undefined) {
          presentWord |= 1 << presenceIndex;
        } else {
          const word = presenceIndex >>> 5;
          presentWords[word] =
            (presentWords[word] ?? 0) | (1 << (presenceIndex & 31));
        }
      }
      (out as unknown as Record<string, unknown>)[field.name] = value;
    }
    setPresentFields(
      out,
      presentWords ?? (presentWord >>> 0),
    );
    return out;
  }

  const count = context.tape[start + 2] ?? 0;
  const data0 = context.tape[start + 3] ?? 0;
  const data1 = context.tape[start + 4] ?? 0;
  if (type.kind === Kind.Array) {
    context.cursor = start + 5;
    if (type.valueKind === Kind.Bool) {
      const out = new Array<boolean>(count);
      for (let index = 0; index < count; index += 1) {
        out[index] = context.input[data0 + index] !== 0;
      }
      return out;
    }
    const out = new Array<unknown>(count);
    if (type.valueKind === Kind.String || type.valueKind === Kind.Bytes) {
      const descriptors = context.cursor;
      context.cursor += count * 2;
      for (let index = 0; index < count; index += 1) {
        out[index] = scalarAt(
          context,
          type.valueKind,
          context.tape[descriptors + index * 2] ?? 0,
          context.tape[descriptors + index * 2 + 1] ?? 0,
        );
      }
    } else if (isComplexKind(type.valueKind)) {
      for (let index = 0; index < count; index += 1) {
        out[index] = materialize(context, type.child);
      }
    } else {
      for (let index = 0; index < count; index += 1) {
        out[index] = scalarAt(
          context,
          type.valueKind,
          data0 + index * data1,
          data1,
        );
      }
    }
    return out;
  }

  const keyWidth = data1 & 0xffff;
  const valueWidth = data1 >>> 16;
  const stride = keyWidth + valueWidth;
  const stringKey = type.keyKind === Kind.String;
  const byteValue =
    type.valueKind === Kind.String || type.valueKind === Kind.Bytes;
  const descriptorStride = (stringKey ? 2 : 0) + (byteValue ? 2 : 0);
  const descriptors = start + 5;
  context.cursor = descriptors + descriptorStride * count;
  const out = new Map<unknown, unknown>();
  for (let index = 0; index < count; index += 1) {
    let descriptor = descriptors + index * descriptorStride;
    const key = stringKey
      ? scalarAt(
          context,
          type.keyKind,
          context.tape[descriptor] ?? 0,
          context.tape[descriptor + 1] ?? 0,
        )
      : scalarAt(
          context,
          type.keyKind,
          data0 + index * stride,
          keyWidth,
        );
    if (stringKey) descriptor += 2;
    let value: unknown;
    if (byteValue) {
      value = scalarAt(
        context,
        type.valueKind,
        context.tape[descriptor] ?? 0,
        context.tape[descriptor + 1] ?? 0,
      );
    } else if (isComplexKind(type.valueKind)) {
      value = materialize(context, type.child);
    } else {
      value = scalarAt(
        context,
        type.valueKind,
        data0 + index * stride + keyWidth,
        valueWidth,
      );
    }
    out.set(key, value);
  }
  return out;
}

export function deserializeWithTape(
  type: RuntimeType,
  input: Uint8Array,
): Message | unknown[] | Map<unknown, unknown> {
  const schemaHandle = compiledSchemaHandle(type);
  const plan = compilePlan(type);
  const range = buildDecodeTape(schemaHandle, input);
  const bytes = new Uint8Array(
    range.memory.buffer,
    range.inputPointer,
    range.inputLength,
  );
  const context: MaterializeContext = {
    input: bytes,
    view: new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength),
    tape: new Uint32Array(
      range.memory.buffer,
      range.tapePointer,
      range.tapeLength / 4,
    ),
    plan,
    cursor: 0,
  };
  const value = materialize(context, plan.root) as
    Message | unknown[] | Map<unknown, unknown>;
  if (context.cursor !== context.tape.length) {
    throw new TypeError("ProtoCache decode tape has trailing records");
  }
  return value;
}
