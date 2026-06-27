#include "webtiles/index/common.h"

#include <algorithm>

namespace webtiles::index {

namespace {

// interleave / deinterleave for 16-bit coordinates

uint32_t deinterleave(uint32_t x)
{
    x = x & 0x55555555;
    x = (x | (x >> 1)) & 0x33333333;
    x = (x | (x >> 2)) & 0x0F0F0F0F;
    x = (x | (x >> 4)) & 0x00FF00FF;
    x = (x | (x >> 8)) & 0x0000FFFF;
    return x;
}

uint32_t interleave(uint32_t x)
{
    x = (x | (x << 8)) & 0x00FF00FF;
    x = (x | (x << 4)) & 0x0F0F0F0F;
    x = (x | (x << 2)) & 0x33333333;
    x = (x | (x << 1)) & 0x55555555;
    return x;
}

} // namespace

uint64_t tilesCountOnZoom(Zoom zoom)
{
    return 1ULL << (2 * zoom);
}

uint64_t encodeTileId(TileId tileId)
{
    // z-order curve (Morton order) encoding
    return interleave(tileId.x) | (interleave(tileId.y) << 1);
}

TileId decodeTileId(uint64_t tileCode, Zoom zoom)
{
    // z-order curve (Morton order) decoding
    return TileId{
        .x = deinterleave(tileCode),
        .y = deinterleave(tileCode >> 1),
        .z = zoom,
    };
}

size_t blockLevelIdx(const ZoomLevels& blockLevels, Zoom zoom)
{
    return std::ranges::find_if(blockLevels, [zoom](Zoom zlevel) { return zoom < zlevel; }) -
           blockLevels.begin() - 1;
}

} // namespace webtiles::index
