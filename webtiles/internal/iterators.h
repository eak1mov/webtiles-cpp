#pragma once

#include "webtiles/common.h"
#include "webtiles/index/index_map.h"

#include <ranges>
#include <utility>

namespace webtiles::internal {

// should not be used directly
using TileLocationsRange = index::IndexMap;

inline auto tilesRange(TileLocationsRange range, FileAccess fileAccess)
{
    return std::views::transform(std::move(range), [fileAccess](const auto& kv) {
        return std::pair{kv.first, fileAccess(kv.second)};
    });
}

using TilesRange = decltype(tilesRange(std::declval<TileLocationsRange>(), {}));

} // namespace webtiles::internal
