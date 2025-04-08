# ProtoCache

Alternative flat binary format for [Protobuf schema](https://protobuf.dev/programming-guides/proto3/). It' works like FlatBuffers, but it's usually smaller and surpports map. Flat means no deserialization overhead. [A benchmark](test/benchmark) shows the Protobuf has considerable deserialization overhead and significant reflection overhead. FlatBuffers is fast but wastes space. ProtoCache takes balance of data size and read speed, so it's useful in data caching.

|  | Protobuf | ProtoCache | FlatBuffers |
|:-------|----:|----:|----:|
| Wire format size | **574B** | 780B | 1296B |
| Decode + Traverse + Dealloc (1 million times) | 1941ms | 154ms | **83ms** |
| Decode + Traverse + Dealloc (1 million times, reflection) | 6127ms | **323ms** | 478ms |

With about 1KB dictionary trained by [random small ProtoCache objects](tools/random-small.cc), ProtoCache+[Zstandard](https://github.com/facebook/zstd) can achieve faster reading and smaller data than Protobuf.

| Level | -1 | 1 | 3 | 5 | 11 | 22 |
|:-------:|:----:|:----:|:----:|:----:|:----:|:----:|
| Compressed data size | 598 | 511 | 515 | 500 | 492 | 486 |
| Decompress (1 million times) | 375ms | 1461ms | 1493ms | 1449ms | 1471ms | 1520ms |


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
auto data = protocache::Serialize(pb_message);
ASSERT_FALSE(data.empty());

// =========basic api=========
auto& root = protocache::Message(data.data()).Cast<test::Main>();
ASSERT_FALSE(!root);

// =========extra api=========
::ex::test::Main ex_root(data.data());

auto copy = ex_root.Serialize();
ASSERT_EQ(data.size(), copy.size());

// deserialize to pb
protocache::Deserialize(protocache::Slice<uint32_t>(data), &pb_mirror);
```
You can create protocache binary by serializing a protobuf message with protocache::Serialize. The Basic API offers fast read-only access with zero-copy technique. Extra APIs provide a mutable object and another serialization method, which only reserialize accessed parts. 

| | Protobuf | ProtoCacheEX | ProtoCache |
|:-------|----:|----:|----:|
| Serialize (1 million times) | 550ms | 360 ~ 2662ms | 7768ms |
| Decode + Traverse + Dealloc (1 million times) | 1941ms | 1134ms | 154ms |

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
The reflection apis are simliar to protobuf's. An example can be found in the [test](test/protocache.cc).

## Other Implements
| Language | Source |
|:----|:----|
| Go | https://github.com/peterrk/protocache-go |
| Java | https://github.com/peterrk/protocache-java |

## Data Size Evaluation
Following work in [paper](https://arxiv.org/pdf/2201.02089.pdf), we can find that protocache has smaller data size than [FlatBuffers](https://flatbuffers.dev/) and [Cap'n Proto](https://capnproto.org/), in most cases.

|  | Protobuf | ProtoCache | FlatBuffers | Cap'n Proto | Protobuf-ZSTD1 | ProtoCache-ZSTD1 | Cap'n Proto (Packed) |
|:-------|----:|----:|----:|----:|----:|----:|----:|
| CircleCI Definition (Blank) | 5 | 8 | 20 | 24 | 14 | 17 | 6 |
| CircleCI Matrix Definition | 26 | 88 | 104 | 96 | 35 | 57 | 36 |
| Entry Point Regulation Manifest | 247 | 352 | 504 | 536 | 191 | 232 | 318 |
| ESLint Configuration Document | 161 | 276 | 320 | 216 | 154 | 132 | 131 |
| ECMAScript Module Loader Definition | 23 | 44 | 80 | 80 | 32 | 53 | 35 |
| GeoJSON Example Document | 325 | 432 | 680 | 448 | 121 | 158 | 228 |
| GitHub FUNDING Sponsorship Definition (Empty) | 17 | 24 | 68 | 40 | 26 | 33 | 25 |
| GitHub Workflow Definition | 189 | 288 | 440 | 464 | 168 | 212 | 242 |
| Grunt.js Clean Task Definition | 20 | 48 | 116 | 96 | 29 | 47 | 39 |
| ImageOptimizer Azure Webjob Configuration | 23 | 60 | 100 | 96 | 32 | 61 | 44 |
| JSON-e Templating Engine Reverse Sort Example | 21 | 68 | 136 | 240 | 30 | 63 | 43 |
| JSON-e Templating Engine Sort Example | 10 | 36 | 44 | 48 | 19 | 45 | 18 |
| JSON Feed Example Document | 413 | 484 | 584 | 568 | 260 | 315 | 470 |
| JSON Resume Example | 2225 | 2608 | 3116 | 3152 | 1408 | 1580 | 2549 |
| .NET Core Project | 284 | 328 | 636 | 608 | 174 | 154 | 376 |
| OpenWeatherMap API Example Document | 188 | 244 | 384 | 320 | 197 | 223 | 206 |
| OpenWeather Road Risk API Example | 173 | 240 | 328 | 296 | 174 | 214 | 204 |
| NPM Package.json Example Manifest | 1581 | 1736 | 2268 | 2216 | 948 | 1020 | 1755 |
| TravisCI Notifications Configuration | 521 | 600 | 668 | 640 | 101 | 117 | 566 |
| TSLint Linter Definition (Basic) | 8 | 24 | 60 | 48 | 17 | 33 | 12 |
| TSLint Linter Definition (Extends Only) | 47 | 68 | 88 | 88 | 19 | 71 | 62 |
| TSLint Linter Definition (Multi-rule) | 14 | 32 | 84 | 80 | 23 | 41 | 23 |
