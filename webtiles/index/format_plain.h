#pragma once

#include "webtiles/common.h"
#include "webtiles/index/index_map.h"

#include <string>
#include <string_view>

namespace webtiles::index {

// Index api for custom file formats

std::string writePlain(const ZoomLevels& blockLevels, const IndexMap& indexMap);

IndexMap readPlain(const ZoomLevels& blockLevels, std::string_view inputData);

PackedLocation queryPlain(
    const ZoomLevels& blockLevels, const FileAccess& fileAccess, TileId tileId);

struct PlainIndexItemLocation {
    PackedLocation blockLocation;
    PackedLocation innerLocation;
};
PlainIndexItemLocation queryPlainBlock(const ZoomLevels& blockLevels, TileId tileId);

} // namespace webtiles::index
