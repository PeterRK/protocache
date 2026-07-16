# ProtoCache

ProtoCache is an alternative [flat binary format](data-format.md) for
[Protobuf schemas](https://protobuf.dev/programming-guides/proto3/). Like
FlatBuffers, it supports direct reads without first materializing an object
graph, while usually producing smaller data and supporting maps. The
[benchmark](test/benchmark) compares data size, traversal, reflection, and
compression costs. ProtoCache is designed for workloads that need a balance
between compact cached data and fast reads.

|  | Protobuf | ProtoCache | FlatBuffers | Cap'n Proto | Fory |
|:-------|----:|----:|----:|----:|-------:|
| Data Size | **574B** | 780B | 1296B | 1288B | 615B |
| Decode + Traverse + Dealloc | 2620ns | 132ns | **89ns** | 610ns | 1748ns |
| Decode + Traverse(reflection) + Dealloc | 8189ns | **270ns** | 484ns | 8748ns | - |
| Compressed/Packed Size | 566B | 571B | 856B | 626B | 611B |
| Compress | 258ns | 427ns | 775ns | - |  295ns | 
| Decompress/Unpack | 114ns | 229ns | 532ns | 653ns | 140ns |

A naive compress algorithm is introduced to reduce continuous `0x00` or `0xff` bytes, which makes the final output size of ProtoCache close to Protobuf. Because Cap'n Proto has a builtin pack algorithm, which shows better compress ratio than our naive compress algorithm, without explicit compress/decompress API, we take the time gap between access in plain and packed mode as decompress time. 

## Difference to Protobuf
Field IDs should not be too sparse. It's illegal to reverse field by assigning a large ID. Normal message should not have any field named `_`, message with only one such field will be considered as an alias. Alias to array or map is useful. We can declare a 2D array like this.
```protobuf
message Vec2D {
	message Vec1D {
		repeated float _ = 1;
	}
	repeated Vec1D _ = 1;
}
```
Some features in Protobuf, like Services, are not supported by ProtoCache. Message defined without any field or message defined with sparse fields, which means too many field ids are missing, are illegal in ProtoCache.

## Build and Install

ProtoCache requires a C++17 compiler and Protobuf. Tests additionally require
GoogleTest, while command-line tools require gflags. CMake 3.13 or newer is
required.

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TEST=ON \
  -DWITH_TOOLS=ON
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/prefix
```

`WITH_BENCHMARK` and `PROTOCACHE_ENABLE_NATIVE_OPT` are disabled by default so
normal builds remain portable. Benchmark builds enable native CPU optimization
and require the additional benchmark libraries used by this repository.

Installed CMake packages can be consumed directly:

```cmake
find_package(ProtoCache 1.2 CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE ProtoCache::protocache)
```

Available library targets are `ProtoCache::protocache` (full static library),
`ProtoCache::protocache-shared`, and `ProtoCache::protocache-lite` (core API
without the reflection extension). When tools are enabled, the install also
contains the JSON converters and all protoc plugins.

The C++, Python, and TypeScript distributions use the same repository release
version. Format compatibility remains governed by [data-format.md](data-format.md)
and cross-language fixtures.

## Code Gen
```sh
protoc --pccx_out=. [--pccx_opt=extra] test.proto
```
A protobuf compiler plugin called `protoc-gen-pccx` is [available](tools/protoc-gen-pccx.cc) to generate header-only C++ file. If option `extra` is set, it will generate another file for extra APIs.

For TypeScript, build the native tools with `WITH_TOOLS=ON`, then run:

```sh
protoc --pcts_out=. test.proto
```

[`protoc-gen-pcts`](tools/protoc-gen-pcts.cc) generates `test.pc.ts`, the public
type/loader entry, and `test.pc.internal.ts`, which contains typed classes,
literal enums, alias schemas, and message metadata. Runtime values are obtained
only after WASM initialization:

```ts
import { loadTest, type Main } from "./test.pc.js";

const generated = await loadTest({ wasm });
const root: Main = generated.Main.deserialize(bytes);
```

Generated files import the npm module `protocache` by default. Use
`--pcts_opt=runtime_import=@scope/package` when the runtime has another module
specifier. See the [TypeScript runtime guide](typescript/README.md) for Node,
browser, module Worker, bundler, and WASM deployment instructions.

## APIs
```cpp
protocache::Buffer buf1;
ASSERT_TRUE(protocache::Serialize(pb_message, &buf1));

// =========basic api=========
auto& root = protocache::Message(buf1.View()).Cast<test::Main>();
ASSERT_FALSE(!root);

// =========extra api=========
::ex::test::Main ex_root(buf1.View());

protocache::Buffer buf2;
ASSERT_TRUE(ex_root.Serialize(&buf2));
ASSERT_EQ(buf1.Size(), buf2.Size());

// deserialize to pb
protocache::Deserialize(data, &pb_mirror);
```
You can create protocache binary by serializing a protobuf message with protocache::Serialize. The Basic API offers fast read-only access with zero-copy technique. Extra APIs provide a mutable object and another serialization method, which only reserialize accessed parts. 

| | Protobuf | ProtoCacheEX | ProtoCache |
|:-------|----:|----:|----:|
| Serialize | **557ns** | 308 ~ 1879ns | 6493ns |
| Decode + Traverse + Dealloc | 2620ns | 1227ns | **132ns** |
| Serialize (twitter.proto) | 215us | **101us** | 412us |

Test full serialization with a complicated `twitter.proto`. Since serializing big object is a memory-bound task, ProtoCache may show it's advantage.

## Reflection
```cpp
std::string err;
google::protobuf::FileDescriptorProto file;
ASSERT_TRUE(protocache::ParseProtoFile("test.proto", &file, &err));

protocache::reflection::DescriptorPool pool;
ASSERT_TRUE(pool.Register(file));

auto descriptor = pool.Find("test.Main");
ASSERT_NE(descriptor, nullptr);
```
The reflection apis are simliar to protobuf's. An example can be found in the [test](test/protocache.cc). If you don't need reflection including the basic serialize API, linking protocache-lite instead of protocache library to avoid dependency on protobuf may be a good idea.

## Other Implements
| Language | Source |
|:----|:----|
| Python | [python](python/README.md) |
| TypeScript | [typescript](typescript) |
| Go | https://github.com/peterrk/protocache-go |
| Java | https://github.com/peterrk/protocache-java |
| C# | https://github.com/peterrk/protocache.net |
| Rust | https://github.com/peterrk/protocache-rust |

## Data Size Evaluation
Following work in [paper](https://arxiv.org/pdf/2201.03051), we can find that protocache has smaller data size than [FlatBuffers](https://flatbuffers.dev/) and [Cap'n Proto](https://capnproto.org/), in most cases.

|  | Protobuf | ProtoCache | FlatBuffers | Cap'n Proto  | ProtoCache (Packed) | Cap'n Proto (Packed) |
|:-------|----:|----:|----:|----:|----:|----:|
| CircleCI Definition (Blank) | 5 | 8 | 20 | 24 | 6 | 6 |
| CircleCI Matrix Definition | 26 | 88 | 104 | 96 | 50 | 36 |
| Entry Point Regulation Manifest | 247 | 352 | 504 | 536 | 303 | 318 |
| ESLint Configuration Document | 161 | 276 | 320 | 216 | 175 | 131 |
| ECMAScript Module Loader Definition | 23 | 44 | 80 | 80 | 33 | 35 |
| GeoJSON Example Document | 325 | 432 | 680 | 448 | 250 | 228 |
| GitHub FUNDING Sponsorship Definition (Empty) | 17 | 24 | 68 | 40 | 23 | 25 |
| GitHub Workflow Definition | 189 | 288 | 440 | 464 | 237 | 242 |
| Grunt.js Clean Task Definition | 20 | 48 | 116 | 96 | 28 | 39 |
| ImageOptimizer Azure Webjob Configuration | 23 | 60 | 100 | 96 | 40 | 44 |
| JSON-e Templating Engine Reverse Sort Example | 21 | 68 | 136 | 240 | 38 | 43 |
| JSON-e Templating Engine Sort Example | 10 | 36 | 44 | 48 | 21 | 18 |
| JSON Feed Example Document | 413 | 484 | 584 | 568 | 474 | 470 |
| JSON Resume Example | 2225 | 2608 | 3116 | 3152 | 2537 | 2549 |
| .NET Core Project | 284 | 328 | 636 | 608 | 303 | 376 |
| OpenWeatherMap API Example Document | 188 | 244 | 384 | 320 | 199 | 206 |
| OpenWeather Road Risk API Example | 173 | 240 | 328 | 296 | 205 | 204 |
| NPM Package.json Example Manifest | 1581 | 1736 | 2268 | 2216 | 1726 | 1755 |
| TravisCI Notifications Configuration | 521 | 600 | 668 | 640 | 601 | 566 |
| TSLint Linter Definition (Basic) | 8 | 24 | 60 | 48 | 14 | 12 |
| TSLint Linter Definition (Extends Only) | 47 | 68 | 88 | 88 | 62 | 62 |
| TSLint Linter Definition (Multi-rule) | 14 | 32 | 84 | 80 | 20 | 23 |
