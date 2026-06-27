#pragma once

#include "webtiles/common.h"

namespace webtiles::index {

uint64_t tilesCountOnZoom(Zoom zoom);

uint64_t encodeTileId(TileId tileId);
TileId decodeTileId(uint64_t tileCode, Zoom zoom);

size_t blockLevelIdx(const ZoomLevels& blockLevels, Zoom zoom);

} // namespace webtiles::index
