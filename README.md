# protocache

Alternative flat binary format for protobuf schema. It' works like flatbuffers, but it's usually smaller than flatbuffers and surpports map.


Following [benchmark](https://github.com/jviotti/binary-json-size-benchmark), we can find that protocache has smaller binary size than flatbuffers and Cap'n Proto, in most cases.

|  | protobuf | protocache | flatbuffers | Cap'n Proto | protobuf-zstd1 | protocache-zstd1 | Cap'n Proto (Packed) |
|:-------|----:|----:|----:|----:|----:|----:|----:|
| CircleCI Definition (Blank) | 5 | 12 | 20 | 24 | 18 | 25 | 6 |
| CircleCI Matrix Definition | 26 | 116 | 104 | 96 | 39 | 88 | 36 |
| Entry Point Regulation Manifest | 247 | 376 | 504 | 536 | 195 | 256 | 318 |
| ESLint Configuration Document | 161 | 296 | 320 | 216 | 158 | 161 | 131 |
| ECMAScript Module Loader Definition | 23 | 48 | 80 | 80 | 36 | 61 | 35 |
| GitHub FUNDING Sponsorship Definition (Empty) | 17 | 28 | 68 | 40 | 30 | 41 | 25 |
| GitHub Workflow Definition | 189 | 328 | 440 | 464 | 170 | 255 | 242 |
| Grunt.js Clean Task Definition | 20 | 60 | 116 | 96 | 33 | 69 | 39 |
| ImageOptimizer Azure Webjob Configuration | 23 | 68 | 100 | 96 | 36 | 71 | 44 |
| JSON-e Templating Engine Reverse Sort Example | 21 | 88 | 136 | 240 | 34 | 80 | 43 |
| JSON-e Templating Engine Sort Example | 10 | 40 | 44 | 48 | 23 | 53 | 18 |
| JSON Feed Example Document | 413 | 496 | 584 | 568 | 264 | 334 | 470 |
| JSON Resume Example | 2225 | 2668 | 3116 | 3152 | 1411 | 1655 | 2549 |
| .NET Core Project | 284 | 352 | 636 | 608 | 178 | 194 | 376 |
| OpenWeatherMap API Example Document | 188 | 264 | 384 | 320 | 201 | 254 | 206 |
| OpenWeather Road Risk API Example | 173 | 264 | 328 | 296 | 178 | 241 | 204 |
| NPM Package.json Example Manifest | 1581 | 1764 | 2268 | 2216 | 952 | 1064 | 1755 |
| TravisCI Notifications Configuration | 521 | 636 | 668 | 640 | 105 | 136 | 566 |
| TSLint Linter Definition (Basic) | 8 | 44 | 60 | 48 | 21 | 57 | 12 |
| TSLint Linter Definition (Extends Only) | 47 | 72 | 88 | 88 | 53 | 79 | 62 |
| TSLint Linter Definition (Multi-rule) | 14 | 52 | 84 | 80 | 27 | 65 | 23 |


## Code Gen
TODO