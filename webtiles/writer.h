#pragma once

#include "webtiles/common.h"

#include <optional>
#include <string>
#include <string_view>

namespace webtiles {

class Writer {
public:
    virtual ~Writer() = default;

    virtual void writeTile(TileId tileId, std::string_view tileData) = 0;
    virtual void writeTile(
        TileId tileId, std::string_view tileData, std::string_view tileDataHash) = 0;

    virtual void finalize() = 0;
};

enum class IndexFormat { BasicPlain = 1, Plain = 2, Sparse = 3 };

struct WriterParams {
    std::string filePath;
    std::string metadata = "";
    std::string extendedHeader = "";
    IndexFormat indexFormat = IndexFormat::Sparse;
};

std::unique_ptr<Writer> createWriter(const WriterParams& writerParams);

} // namespace webtiles
