# ProtoCache

Alternative flat binary format for [Protobuf schema](https://protobuf.dev/programming-guides/proto3/). It' works like FlatBuffers, but it's usually smaller and surpports map. Flat means [no deserialization overhead](https://flatbuffers.dev/flatbuffers_benchmarks.html).

## Difference to Protobuf
Field IDs should not be too sparse. It's illegal to reverse field by assigning a large ID. Normal message should not have any field named "_", message with only one such field will be considered as an alias. Alias for array or map is useful. We can declare a 2D array like this.
```protobuf
message Vec2D {
	message Vec1D {
		repeated float _ = 1;
	}
	repeated Vec1D _ = 1;
}
```
Some features in Protobuf, like Services, are not supported.

## Code Gen
```sh
protoc --pccx_out=. test.proto
```
A protobuf compiler plugin called "protoc-gen-pccx" is available to generate header-only C++ file. The generated file is short and human friendly. Howerver, there is a known flaw that type declaration order may break C++ compilation. Don't mind to edit it if nessasery.


## Binary Size

Following work in [paper](https://arxiv.org/pdf/2201.02089.pdf), we can find that protocache has smaller binary size than [FlatBuffers](https://flatbuffers.dev/) and [Cap'n Proto](https://capnproto.org/), in most cases.

|  | Protobuf | ProtoCache | FlatBuffers | Cap'n Proto | Protobuf-ZSTD1 | ProtoCache-ZSTD1 | Cap'n Proto (Packed) |
|:-------|----:|----:|----:|----:|----:|----:|----:|
| CircleCI Definition (Blank) | 5 | 8 | 20 | 24 | 18 | 21 | 6 |
| CircleCI Matrix Definition | 26 | 88 | 104 | 96 | 39 | 61 | 36 |
| Entry Point Regulation Manifest | 247 | 352 | 504 | 536 | 195 | 236 | 318 |
| ESLint Configuration Document | 161 | 276 | 320 | 216 | 158 | 136 | 131 |
| ECMAScript Module Loader Definition | 23 | 44 | 80 | 80 | 36 | 57 | 35 |
| GeoJSON Example Document | 325 | 432 | 680 | 448 | 128 | 165 | 228 | 165 |
| GitHub FUNDING Sponsorship Definition (Empty) | 17 | 24 | 68 | 40 | 30 | 37 | 25 |
| GitHub Workflow Definition | 189 | 288 | 440 | 464 | 170 | 212 | 242 |
| Grunt.js Clean Task Definition | 20 | 48 | 116 | 96 | 33 | 51 | 39 |
| ImageOptimizer Azure Webjob Configuration | 23 | 60 | 100 | 96 | 36 | 65 | 44 |
| JSON-e Templating Engine Reverse Sort Example | 21 | 68 | 136 | 240 | 34 | 67 | 43 |
| JSON-e Templating Engine Sort Example | 10 | 36 | 44 | 48 | 23 | 49 | 18 |
| JSON Feed Example Document | 413 | 484 | 584 | 568 | 264 | 319 | 470 |
| JSON Resume Example | 2225 | 2608 | 3116 | 3152 | 1411 | 1584 | 2549 |
| .NET Core Project | 284 | 328 | 636 | 608 | 178 | 158 | 376 |
| OpenWeatherMap API Example Document | 188 | 244 | 384 | 320 | 201 | 227 | 206 |
| OpenWeather Road Risk API Example | 173 | 240 | 328 | 296 | 178 | 218 | 204 |
| NPM Package.json Example Manifest | 1581 | 1736 | 2268 | 2216 | 952 | 1021 | 1755 |
| TravisCI Notifications Configuration | 521 | 600 | 668 | 640 | 105 | 121 | 566 |
| TSLint Linter Definition (Basic) | 8 | 24 | 60 | 48 | 21 | 37 | 12 |
| TSLint Linter Definition (Extends Only) | 47 | 68 | 88 | 88 | 53 | 75 | 62 |
| TSLint Linter Definition (Multi-rule) | 14 | 32 | 84 | 80 | 27 | 45 | 23 |