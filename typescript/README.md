# ProtoCache for TypeScript

`protocache` is the typed TypeScript runtime for the
[ProtoCache](https://github.com/PeterRK/ProtoCache) flat binary format. It
fully materializes messages as ordinary JavaScript objects and uses the same
C++ codec algorithms through a small WebAssembly module.

The package is ESM-only and requires a WebAssembly-capable runtime. Node.js 18
or newer is supported. Browser and module Worker applications can use the
same package when they serve `protocache.wasm` as a static asset.

## Install

```sh
npm install protocache
```

Generated message modules are produced by the native `protoc-gen-pcts`
plugin from the main ProtoCache repository:

```sh
protoc --pcts_out=. schema.proto
```

The generator emits a public `schema.pc.ts` loader and an internal
`schema.pc.internal.ts` implementation. Its default runtime import is
`protocache`; use `--pcts_opt=runtime_import=@scope/package` when publishing
the runtime under another package name.

## Node.js

Load the packaged WASM file as bytes before asking a generated module for its
runtime values:

```ts
import { createRequire } from "node:module";
import { readFile } from "node:fs/promises";
import { loadSchema, type Main } from "./schema.pc.js";

const require = createRequire(import.meta.url);
const wasm = await readFile(require.resolve("protocache/protocache.wasm"));
const generated = await loadSchema({ wasm });

const value: Main = new generated.Main({ id: 7n });
const bytes = value.serialize();
const decoded = generated.Main.deserialize(bytes);
```

`loadSchema()` initializes the process-wide runtime before dynamically
loading generated constructors. Concurrent calls share one promise. A failed
initialization can be retried with new options.

## Browser and module Worker

Copy `node_modules/protocache/dist/protocache.wasm` into the application's
static assets and pass its deployed URL:

```ts
import { init, arraySchemaV1, deserialize, serialize, Kind } from "protocache";

await init({ wasmUrl: "/assets/protocache.wasm" });

const schema = arraySchemaV1<number>(Kind.I32);
const bytes = serialize(schema, [1, 2, 3]);
const values = deserialize(schema, bytes);
```

The same code works in a module Worker. Initialization is global to one JavaScript
realm, so the window and each Worker initialize their own WASM instance.

When a deployment cannot use `fetch`, pass a `WebAssembly.Module`,
`ArrayBuffer`, or typed-array view through the `wasm` option instead. Specify
exactly one of `wasm` and `wasmUrl`.

## Messages and standalone containers

Generated classes extend `Message` and expose `serialize()`, static
`deserialize()`, and `hasField(name)`. Repeated fields use `Array<T>`, maps use
`Map<K, V>`, bytes use `Uint8Array`, and signed or unsigned 64-bit integers use
`bigint`.

Standalone alias containers are represented by a native value plus a generated
schema token:

```ts
const encoded = serialize(generated.Vec2DSchema, [[1, 2], [3]]);
const decoded = deserialize(generated.Vec2DSchema, encoded);
```

`compress()` and `decompress()` provide synchronous bytes APIs after the same
initialization step.

## Development

An Emscripten SDK is required to rebuild the WASM artifact. Set `EMSDK` or
`EMCMAKE`, or place an activated SDK under `.tools/emsdk`.

```sh
npm ci
npm run typecheck
npm test
npm run build
npm pack --dry-run
```

`npm test` runs the TypeScript runtime suite, real WASM smoke test, native C++
adapter test, Node module Worker test, esbuild browser bundle test, and a real
headless browser test covering both the window and a module Web Worker. Install
Firefox or Chromium, or set `PROTOCACHE_TEST_BROWSER` to the browser executable.
The `test:worker`, `test:bundler`, `test:browser`, `test:wasm`, and
`test:wasm-native` scripts remain available for focused runs.

## Scope

This binding intentionally provides a fully materialized mutable object model.
It does not currently expose the C++ binding's zero-copy view API, mutable lazy
EX API, runtime `.proto` reflection, protobuf message bridge, services, or RPC.

## License

BSD-3-Clause. See the repository's
[LICENSE](https://github.com/PeterRK/ProtoCache/blob/main/LICENSE).
