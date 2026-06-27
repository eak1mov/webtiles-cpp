#pragma once

#include "webtiles/common.h"
#include "webtiles/index/index_map.h"

#include <string>
#include <string_view>

namespace webtiles::index {

// Index api for custom file formats

IndexData writeSparse(const ZoomLevels& blockLevels, IndexMap indexMap);

IndexMap readSparse(
    const ZoomLevels& blockLevels, PackedLocation rootLocation, std::string_view inputData);

PackedLocation querySparse(
    const ZoomLevels& blockLevels,
    PackedLocation rootLocation,
    const FileAccess& fileAccess,
    TileId tileId);

PackedLocation querySparseBlock(
    const ZoomLevels& blockLevels, size_t blockIdx, std::string_view blockData, TileId tileId);

} // namespace webtiles::index
