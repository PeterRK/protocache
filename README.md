# ProtoCache

Alternative [flat binary format](data-format.md) for [Protobuf schema](https://protobuf.dev/programming-guides/proto3/). It' works like FlatBuffers, but it's usually smaller and surpports map. Flat means no deserialization overhead. [A benchmark](test/benchmark) shows the Protobuf has considerable deserialization overhead and significant reflection overhead. FlatBuffers is fast but wastes space. ProtoCache takes balance of data size and read speed, so it's useful in data caching.

|  | Protobuf | ProtoCache | FlatBuffers | Cap'n Proto |
|:-------|----:|----:|----:|----:|
| Data Size | **574B** | 780B | 1296B | 1288B |
| Decode + Traverse + Dealloc | 1941ns | 140ns | **83ns** | 558ns |
| Decode + Traverse(reflection) + Dealloc | 6127ns | **269ns** | 478ns | 7061ns |
| Compressed/Packed Size | 566B | 571B | 856B | 626B |
| Compress | 257ns | 401ns | 763ns | - |
| Decompress/Unpack | 107ns | 250ns | 561ns | 641ns |

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

## Code Gen
```sh
protoc --pccx_out=. [--pccx_opt=extra] test.proto
```
A protobuf compiler plugin called `protoc-gen-pccx` is [available](tools/protoc-gen-pccx.cc) to generate header-only C++ file. If option `extra` is set, it will generate another file for extra APIs.

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
| Serialize | **542ns** | 350 ~ 2123ns | 6463ns |
| Decode + Traverse + Dealloc | 1941ns | 1134ns | **154ns** |
| Serialize (twitter.proto) | 214us | **125us** | 472us |

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
| Go | https://github.com/peterrk/protocache-go |
| Java | https://github.com/peterrk/protocache-java |
| C# | https://github.com/peterrk/protocache.net |

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
