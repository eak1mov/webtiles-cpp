#pragma once

#include "webtiles/common.h"

#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"

namespace webtiles::index {

struct TileIdHash {
    size_t operator()(TileId tileId) const { return absl::HashOf(tileId.x, tileId.y, tileId.z); }
};

using IndexMap = absl::flat_hash_map<TileId, PackedLocation, TileIdHash>;

struct IndexData {
    std::string data;
    PackedLocation rootLocation;
};

} // namespace webtiles::index
