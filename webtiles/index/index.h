#pragma once

#include "webtiles/common.h"
#include "webtiles/fbs/header_generated.h"
#include "webtiles/index/index_map.h"

#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"

#include <string>
#include <string_view>

namespace webtiles::index {

// Index api for custom file formats

IndexData writeIndex(const fbs::IndexHeader& header, IndexMap indexMap);

IndexMap readIndex(const fbs::IndexHeader& header, std::string_view indexData);

PackedLocation queryIndex(
    const fbs::IndexHeader& header, const FileAccess& fileAccess, TileId tileId);

} // namespace webtiles::index
