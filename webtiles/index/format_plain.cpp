#include "webtiles/index/format_plain.h"

#include "webtiles/common.h"
#include "webtiles/index/common.h"
#include "webtiles/internal/asserts.h"

namespace webtiles::index {

namespace {

uint64_t calcBlockSize(uint32_t zoomCount)
{
    // = 4^0 + 4^1 + ... + 4^(zoomCount-1)
    return ((1ULL << (2 * zoomCount)) - 1) / 3;
}

} // namespace

std::string writePlain(const ZoomLevels& blockLevels, const IndexMap& indexMap)
{
    std::string result;
    result.resize(calcBlockSize(blockLevels.back()) * sizeof(PackedLocation));

    for (auto [tileId, tileLocation] : indexMap) {
        auto [blockLocation, innerLocation] = queryPlainBlock(blockLevels, tileId);
        uint64_t offset = blockLocation.offset + innerLocation.offset;

        PackedLocation packedLocation = tileLocation;
        memcpy(&result[offset], &packedLocation, sizeof(packedLocation));
    }

    return result;
}

IndexMap readPlain(const ZoomLevels& blockLevels, std::string_view inputData)
{
    IndexMap result;
    result.reserve(inputData.size() / sizeof(PackedLocation));

    size_t offset = 0;

    for (size_t bIdx = 0; bIdx + 1 < blockLevels.size(); ++bIdx) {
        uint32_t blockZ = blockLevels[bIdx];
        uint32_t blockCount = tilesCountOnZoom(blockZ);
        uint32_t innerZCount = blockLevels[bIdx + 1] - blockLevels[bIdx];

        for (uint32_t blockCode = 0; blockCode < blockCount; ++blockCode) {
            TileId blockTileId = decodeTileId(blockCode, blockZ);

            for (uint32_t innerZ = 0; innerZ < innerZCount; ++innerZ) {
                uint32_t innerCount = tilesCountOnZoom(innerZ);

                for (uint32_t innerCode = 0; innerCode < innerCount; ++innerCode) {
                    TileId innerTileId = decodeTileId(innerCode, innerZ);
                    TileId tileId = {
                        .x = (blockTileId.x << innerZ) + innerTileId.x,
                        .y = (blockTileId.y << innerZ) + innerTileId.y,
                        .z = blockZ + innerZ,
                    };

                    PackedLocation packedLocation{};
                    memcpy(&packedLocation, &inputData[offset], sizeof(packedLocation));
                    offset += sizeof(packedLocation);

                    if (packedLocation.size) {
                        result.emplace(tileId, packedLocation);
                    }
                }
            }
        }
    }

    return result;
}

PlainIndexItemLocation queryPlainBlock(const ZoomLevels& blockLevels, TileId tileId)
{
    size_t blockIdx = blockLevelIdx(blockLevels, tileId.z);
    uint32_t blockZ = blockLevels[blockIdx];
    uint32_t innerZ = tileId.z - blockZ;
    uint32_t innerZCount = blockLevels[blockIdx + 1] - blockLevels[blockIdx];

    TileId blockTileId = {
        .x = tileId.x >> innerZ,
        .y = tileId.y >> innerZ,
        .z = tileId.z - innerZ,
    };
    TileId innerTileId = {
        .x = tileId.x & ((1U << innerZ) - 1),
        .y = tileId.y & ((1U << innerZ) - 1),
        .z = innerZ,
    };

    uint64_t blockCode = encodeTileId(blockTileId);
    uint64_t blockSize = calcBlockSize(innerZCount);
    uint64_t blockOffset = calcBlockSize(blockZ) + blockCode * blockSize;

    uint64_t innerCode = encodeTileId(innerTileId);
    uint64_t innerSize = 1;
    uint64_t innerOffset = calcBlockSize(innerZ) + innerCode * innerSize;

    return {
        .blockLocation =
            {.offset = blockOffset * sizeof(PackedLocation),
             .size = blockSize * sizeof(PackedLocation)},
        .innerLocation =
            {.offset = innerOffset * sizeof(PackedLocation),
             .size = innerSize * sizeof(PackedLocation)},
    };
}

PackedLocation queryPlain(
    const ZoomLevels& blockLevels, const FileAccess& fileAccess, TileId tileId)
{
    auto [blockLocation, innerLocation] = queryPlainBlock(blockLevels, tileId);

    auto blockData = fileAccess(blockLocation);
    auto indexItemData =
        std::string_view(blockData).substr(innerLocation.offset, innerLocation.size);

    WT_ASSERT(indexItemData.size() == sizeof(PackedLocation));
    PackedLocation packedLocation{};
    memcpy(&packedLocation, indexItemData.data(), sizeof(packedLocation));
    return packedLocation;
}

} // namespace webtiles::index
