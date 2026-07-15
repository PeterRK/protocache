import * as pc from "../src/index.js";
import {
  loadTest,
  type Main as LoadedMain,
} from "./generated/test.pc.js";
import {
  ArrMapSchema,
  Main,
  Mode,
  Small,
  Vec2D_Vec1DSchema,
  type Mode as ModeValue,
} from "./generated/test.pc.internal.js";

const knownMode: ModeValue = Mode.MODE_B;
const futureMode: ModeValue = pc.unknownEnum(123);

const testApiPromise = loadTest({ wasm: new Uint8Array() });
// @ts-expect-error generated constructor values require awaiting loadTest
testApiPromise.Main;
async function loadMain(): Promise<LoadedMain> {
  const test = await testApiPromise;
  return new test.Main();
}
void loadMain;

new Main({
  i32: 1,
  i64: 2n,
  mode: knownMode,
  modev: [futureMode],
  object: new Small({ str: "nested" }),
});

new Main().hasField("object");

function deserializeMain(input: Uint8Array): Main {
  return Main.deserialize(input);
}
void deserializeMain;

const serializedMain: Uint8Array = new Main().serialize();
const serializedRow: Uint8Array = pc.serialize(Vec2D_Vec1DSchema, [1, 2]);
const compressed: Uint8Array = pc.compress(serializedMain);
const decompressed: Uint8Array = pc.decompress(compressed);
void serializedMain;
void serializedRow;
void decompressed;

// @ts-expect-error unknown generated field
new Main({ missing: 1 });

// @ts-expect-error i64 fields require bigint
new Main({ i64: 1 });

// @ts-expect-error arbitrary numbers are not typed enum values
new Main({ mode: 123 });

// @ts-expect-error hasField accepts generated data field names only
new Main().hasField("missing");

// @ts-expect-error methods are not data fields
new Main().hasField("hasField");

// @ts-expect-error number elements cannot use the string kind
pc.arraySchemaV1<number>(pc.Kind.String);

// @ts-expect-error string map keys cannot use the i32 key kind
pc.mapSchemaV1<string, number>(pc.Kind.I32, pc.Kind.I32);

const validFields = [
  ["i64", 2, false, pc.Kind.None, pc.Kind.I64],
] satisfies pc.MessageFieldsV1<Main>;
void validFields;

// @ts-expect-error field name must name a generated data property
const badName: pc.MessageFieldsV1<Main> = [["missing", 0, false, pc.Kind.None, pc.Kind.I32]];

// @ts-expect-error bigint field metadata cannot use an i32 kind
const badI64: pc.MessageFieldsV1<Main> = [["i64", 2, false, pc.Kind.None, pc.Kind.I32]];

// @ts-expect-error repeated number field metadata cannot use string
const badRepeated: pc.MessageFieldsV1<Main> = [["i32v", 11, true, pc.Kind.None, pc.Kind.String]];

void ArrMapSchema;
void Vec2D_Vec1DSchema;
void badName;
void badI64;
void badRepeated;
