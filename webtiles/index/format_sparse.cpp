#include "webtiles/index/format_sparse.h"

#include "webtiles/common.h"
#include "webtiles/fbs/sparse_block_generated.h"
#include "webtiles/index/common.h"
#include "webtiles/index/format_sparse_block.h"
#include "webtiles/internal/asserts.h"
#include "webtiles/internal/digest.h"

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

#include <bit>

namespace webtiles::index {

namespace {

static_assert(sizeof(PackedLocation) == sizeof(fbs::Location));

fbs::Location castFbs(PackedLocation location)
{
    return std::bit_cast<fbs::Location>(location);
}

PackedLocation castFbs(fbs::Location location)
{
    return std::bit_cast<PackedLocation>(location);
}

flatbuffers::DetachedBuffer writeBlock(const fbs::SparseBlockT& block)
{
    flatbuffers::FlatBufferBuilder fbb;
    fbb.Finish(fbs::SparseBlock::Pack(fbb, &block));
    return fbb.Release();
}

} // namespace

IndexData writeSparse(const ZoomLevels& blockLevels, IndexMap indexMap)
{
    std::string result;
    result.reserve(indexMap.size() * sizeof(fbs::Location));

    // blockIdx -> [blockTileId]
    std::vector<absl::flat_hash_set<TileId, TileIdHash>> usedBlocks(blockLevels.size() - 1);

    for (auto [tileId, tileLocation] : indexMap) {
        size_t maxBlockIdx = blockLevelIdx(blockLevels, tileId.z);
        for (size_t bIdx = 0; bIdx <= maxBlockIdx; ++bIdx) {
            uint32_t blockZ = blockLevels[bIdx];
            uint32_t innerZ = tileId.z - blockZ;
            TileId blockTileId = {
                .x = tileId.x >> innerZ,
                .y = tileId.y >> innerZ,
                .z = tileId.z - innerZ,
            };
            usedBlocks[bIdx].insert(blockTileId);
        }
    }

    uint64_t outputOffset = 0;
    absl::flat_hash_map<std::string, PackedLocation> hash2location;

    for (size_t bIdxRev = 0; bIdxRev < usedBlocks.size(); ++bIdxRev) {
        size_t bIdx = usedBlocks.size() - 1 - bIdxRev;
        uint32_t blockZ = blockLevels[bIdx];

        uint32_t innerZCount = blockLevels[bIdx + 1] - blockLevels[bIdx];
        if (bIdx + 1 != blockLevels.size() - 1) {
            ++innerZCount; // locations of the next block (blockLocation)
        }

        for (TileId blockTileId : usedBlocks[bIdx]) {
            fbs::SparseBlockT block;
            block.block_type = fbs::BlockType_Dense;
            for (uint32_t innerZ = 0; innerZ < innerZCount; ++innerZ) {
                block.dense_locations.push_back(std::make_unique<fbs::DenseLocationsT>());
            }

            for (uint32_t innerZ = 0; innerZ < innerZCount; ++innerZ) {
                uint32_t innerCount = tilesCountOnZoom(innerZ);

                auto& locations = block.dense_locations[innerZ]->locations;
                locations.resize(innerCount);

                for (uint64_t innerCode = 0; innerCode < innerCount; ++innerCode) {
                    TileId innerTileId = decodeTileId(innerCode, innerZ);
                    TileId tileId = {
                        .x = (blockTileId.x << innerZ) + innerTileId.x,
                        .y = (blockTileId.y << innerZ) + innerTileId.y,
                        .z = blockZ + innerZ,
                    };
                    if (auto it = indexMap.find(tileId); it != indexMap.end()) {
                        locations[innerCode] = castFbs(it->second);
                    }
                }
            }

            flatbuffers::DetachedBuffer denseBuffer = writeBlock(block);
            std::string denseBufferHash = internal::computeHash(denseBuffer);

            if (auto it = hash2location.find(denseBufferHash); it != hash2location.end()) {
                indexMap[blockTileId] = it->second;
                continue;
            }

            block = denseToSparse(block);
            flatbuffers::DetachedBuffer sparseBuffer = writeBlock(block);
            std::string sparseBufferHash = internal::computeHash(sparseBuffer);

            if (auto it = hash2location.find(sparseBufferHash); it != hash2location.end()) {
                indexMap[blockTileId] = it->second;
                continue;
            }

            auto& buffer = (denseBuffer.size() <= sparseBuffer.size()) ? denseBuffer : sparseBuffer;

            PackedLocation blockLocation = {
                .offset = outputOffset,
                .size = buffer.size(),
            };
            result.append(reinterpret_cast<char*>(buffer.data()), buffer.size());
            outputOffset += buffer.size();

            indexMap[blockTileId] = blockLocation;
            hash2location.insert({denseBufferHash, blockLocation});
            hash2location.insert({sparseBufferHash, blockLocation});
        }
    }

    PackedLocation rootLocation{};
    if (auto it = indexMap.find({0, 0, 0}); it != indexMap.end()) {
        rootLocation = it->second;
    }

    return {
        .data = std::move(result),
        .rootLocation = rootLocation,
    };
}

IndexMap readSparse(
    const ZoomLevels& blockLevels, PackedLocation rootLocation, std::string_view inputData)
{
    IndexMap result;
    result.reserve(inputData.size() / sizeof(fbs::Location));

    result[{0, 0, 0}] = rootLocation;

    for (size_t bIdx = 0; bIdx + 1 < blockLevels.size(); ++bIdx) {
        uint32_t blockZ = blockLevels[bIdx];
        uint64_t blockCount = tilesCountOnZoom(blockZ);

        uint32_t innerZCount = blockLevels[bIdx + 1] - blockLevels[bIdx];
        if (bIdx + 1 != blockLevels.size() - 1) {
            ++innerZCount; // locations of the next block (blockLocation)
        }

        for (uint64_t blockCode = 0; blockCode < blockCount; ++blockCode) {
            TileId blockTileId = decodeTileId(blockCode, blockZ);

            PackedLocation blockLocation{};
            if (auto it = result.find(blockTileId); it != result.end()) {
                blockLocation = it->second;
                result.erase(it);
            }

            if (!blockLocation.size) {
                continue;
            }

            fbs::SparseBlockT block;
            fbs::GetSparseBlock(inputData.data() + blockLocation.offset)->UnPackTo(&block);

            switch (block.block_type) {
                case fbs::BlockType_Dense:
                    break;
                case fbs::BlockType_Sparse:
                    block = sparseToDense(block);
                    break;
                default:
                    throw InvalidDatasetError(
                        "Invalid block type " + std::to_string(block.block_type));
            }

            for (uint32_t innerZ = 0; innerZ < innerZCount; ++innerZ) {
                uint32_t innerCount = tilesCountOnZoom(innerZ);

                const auto& locations = block.dense_locations.at(innerZ)->locations;

                for (uint32_t innerCode = 0; innerCode < innerCount; ++innerCode) {
                    TileId innerTileId = decodeTileId(innerCode, innerZ);
                    TileId tileId = {
                        .x = (blockTileId.x << innerZ) + innerTileId.x,
                        .y = (blockTileId.y << innerZ) + innerTileId.y,
                        .z = blockZ + innerZ,
                    };
                    PackedLocation tileLocation = castFbs(locations.at(innerCode));
                    if (!tileLocation.size) {
                        continue;
                    }
                    result[tileId] = tileLocation;
                }
            }
        }
    }

    return result;
}

PackedLocation querySparseBlock(
    const ZoomLevels& blockLevels, size_t blockIdx, std::string_view blockData, TileId tileId)
{
    WT_ASSERT(blockIdx <= blockLevelIdx(blockLevels, tileId.z));
    WT_ASSERT(!blockData.empty());

    uint32_t tileZ = tileId.z;
    uint32_t blockZ = blockLevels[blockIdx];
    uint32_t nextBlockZ = blockLevels[blockIdx + 1];

    uint32_t nextZ = std::min(nextBlockZ, tileZ);
    uint32_t nextSize = nextZ - blockZ;
    uint32_t nextMask = (1U << nextSize) - 1;

    // smaller zoom levels correspond to top bits of x/y coordinates
    // top block = top bits of coordinate, bottom block = bottom bits of coordinate
    TileId innerTileId = {
        .x = (tileId.x >> (tileZ - nextZ)) & nextMask,
        .y = (tileId.y >> (tileZ - nextZ)) & nextMask,
        .z = nextSize,
    };

    const fbs::SparseBlock* block = fbs::GetSparseBlock(blockData.data());

    return castFbs(queryBlock(block, innerTileId));
}

PackedLocation querySparse(
    const ZoomLevels& blockLevels,
    PackedLocation rootLocation,
    const FileAccess& fileAccess,
    TileId tileId)
{
    PackedLocation location = rootLocation;

    size_t maxBlockIdx = blockLevelIdx(blockLevels, tileId.z);
    for (size_t blockIdx = 0; blockIdx <= maxBlockIdx; ++blockIdx) {
        if (!location.size) {
            return location;
        }
        auto blockData = fileAccess(location);
        location = querySparseBlock(blockLevels, blockIdx, blockData, tileId);
    }

    return location;
}

} // namespace webtiles::index
