# webtiles-cpp

[![MIT License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++ Version](https://img.shields.io/badge/C%2B%2B-20-brightgreen.svg)](https://en.cppreference.com/w/cpp/20.html)

C++ implementation of [WebTiles](https://github.com/eak1mov/webtiles) tile storage format.

## Prerequisites

- [Bazel](https://bazel.build/install) build system.
- C++20 compatible toolchain.
- Dependencies are managed by Bazel (abseil, flatbuffers).

## Building

```bash
# Clone the repository
git clone https://github.com/eak1mov/webtiles-cpp.git
cd webtiles-cpp

# Build everything
bazel build //...

# Run tests
bazel test //...
bazel run //webtiles:tests
```

## Command Line Tools (examples)

```bash
# Build all converters
bazel build //converter:all

# Convert mbtiles to webtiles:
bazel-bin/converter/import_mbtiles --input_path=input.mbtiles --output_path=output.wtiles

# Convert webtiles to mbtiles:
bazel-bin/converter/export_mbtiles --input_path=input.wtiles --output_path=output.mbtiles

# Export webtiles file to directory of files:
bazel-bin/converter/export_xyz --input_path=input.wtiles --output_path="tiles/{z}/{x}/{y}.png"

# Import directory of files into webtiles file:
bazel-bin/converter/import_xyz --input_path="tiles/{z}/{x}/{y}.png" --output_path=output.wtiles
```

## Usage Example

```cpp
#include "webtiles/reader.h"
#include "webtiles/writer.h"

webtiles::WriterParams params = {
    .filePath = "output.wtiles",
    .metadata = R"({"foo": "bar"})",
    .indexFormat = webtiles::IndexFormat::Sparse,
};
auto writer = webtiles::createWriter(params);
writer->writeTile({.x = 0, .y = 0, .z = 0}, "tile data");
writer->finalize();

auto reader = webtiles::createFileReader("output.wtiles");
auto tileData = reader->readTileData({.x = 0, .y = 0, .z = 0});

for (const auto& [tileId, tileData] : reader->allTiles()) {
    // process tileId (x, y, z) and tileData
}
```
