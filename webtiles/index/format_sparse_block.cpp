#include "webtiles/index/format_sparse_block.h"

#include "webtiles/common.h"
#include "webtiles/index/common.h"
#include "webtiles/internal/asserts.h"

#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"

#include <bit>
#include <cassert>

namespace webtiles::index {

namespace {

TileId parent(TileId tileId)
{
    return {
        .x = tileId.x >> 1,
        .y = tileId.y >> 1,
        .z = tileId.z - 1,
    };
}

uint64_t parent(uint64_t tileCode)
{
    return tileCode >> 2;
}

size_t childCountAt(Zoom zoom, Zoom maxZoom)
{
    return zoom < maxZoom ? 4 : 0;
}

uint64_t childCodeAt(uint64_t tileCode, uint32_t childIdx)
{
    return (tileCode << 2) + childIdx;
}

// Returns a tile in the `newRoot` subtree with the same path as `tile` in the `oldRoot` subtree.
// Returned value will have higher bits from `newRoot`, and lower bits from `tile`:
//                    higher   lower
//                     bits     bits
// oldRoot:         [RRRRRRRR]
// newRoot:         [NNNNNNNN]
// tile:            [RRRRRRRRTTTTTTTT]
// returned tileId: [NNNNNNNNTTTTTTTT]
TileId changeRoot(TileId tile, TileId oldRoot, TileId newRoot)
{
    // `oldRoot` and `newRoot` must be on the same z level
    assert(oldRoot.z == newRoot.z);
    // `tile` must be in the `oldRoot` subtree
    assert(oldRoot.z <= tile.z);

    uint32_t zDiff = tile.z - oldRoot.z;

    // `tile` must be in the `oldRoot` subtree (have the same higher bits)
    assert(oldRoot.x == (tile.x >> zDiff));
    assert(oldRoot.y == (tile.y >> zDiff));

    uint32_t mask = (1U << zDiff) - 1;
    return {
        .x = (newRoot.x << zDiff) + (tile.x & mask),
        .y = (newRoot.y << zDiff) + (tile.y & mask),
        .z = newRoot.z + zDiff,
    };
}

struct LocationHash {
    size_t operator()(fbs::Location location) const
    {
        return absl::HashOf(std::bit_cast<uint64_t>(location));
    }
};

struct LocationEq {
    bool operator()(fbs::Location lhs, fbs::Location rhs) const
    {
        return std::bit_cast<uint64_t>(lhs) == std::bit_cast<uint64_t>(rhs);
    }
};

struct TileClasses {
    uint32_t tileClass;
    std::vector<uint32_t> childClasses;

    bool operator==(const TileClasses&) const = default;
};

struct TileClassesHash {
    size_t operator()(const TileClasses& tile) const
    {
        return absl::HashOf(tile.tileClass, tile.childClasses);
    }
};

// for each tile return a small integer value equivalent to tile location
// (equivalence class number of the tile)
std::vector<uint32_t> calcEqClasses(const std::vector<fbs::Location>& locations)
{
    // Location -> class
    absl::flat_hash_map<fbs::Location, uint32_t, LocationHash, LocationEq> usedLocations;
    usedLocations.reserve(locations.size());

    // tileCode -> class
    std::vector<uint32_t> classes(locations.size());

    for (uint64_t tileCode = 0; tileCode < locations.size(); ++tileCode) {
        auto [it, _] = usedLocations.try_emplace(locations[tileCode], usedLocations.size());
        classes[tileCode] = it->second;
    }

    return classes;
}

} // namespace

fbs::SparseBlockT denseToSparse(const fbs::SparseBlockT& block)
{
    uint32_t zCount = block.dense_locations.size();

    WT_ASSERT(block.block_type == fbs::BlockType_Dense);
    WT_ASSERT(!block.dense_locations.empty());
    for (const auto& denseLocations : block.dense_locations) {
        WT_ASSERT(denseLocations);
    }

    fbs::SparseBlockT result;
    result.block_type = fbs::BlockType_Sparse;
    for (uint32_t z = 0; z < zCount; ++z) {
        result.sparse_locations.push_back(std::make_unique<fbs::SparseLocationsT>());
    }

    const auto& denseLocations = block.dense_locations;
    const auto& sparseLocations = result.sparse_locations;

    // z -> (tileCode -> eqClass)
    std::vector<std::vector<uint32_t>> eqClasses(zCount);

    for (uint32_t z = zCount - 1; z > 0; --z) {
        const auto& locations = denseLocations[z]->locations;

        eqClasses[z] = calcEqClasses(locations);
        size_t tilesCount = eqClasses[z].size();
        size_t childCount = childCountAt(z, /*maxZoom=*/zCount - 1);

        absl::flat_hash_map<TileClasses, uint32_t, TileClassesHash> curClasses;
        curClasses.reserve(tilesCount);

        TileClasses tile{};
        tile.childClasses.resize(childCount);

        for (uint32_t tileCode = 0; tileCode < tilesCount; ++tileCode) {
            tile.tileClass = eqClasses[z][tileCode];
            for (uint32_t childIdx = 0; childIdx < childCount; ++childIdx) {
                uint32_t childCode = childCodeAt(tileCode, childIdx);
                tile.childClasses[childIdx] = eqClasses[z + 1][childCode];
            }
            auto [it, inserted] = curClasses.try_emplace(tile, curClasses.size());
            eqClasses[z][tileCode] = it->second;
        }
    }
    eqClasses[0] = {0}; // there is only one tile on zoom #0

    // z -> (tileCode -> link status)
    std::vector<std::vector<bool>> linkStatus(zCount);

    for (uint32_t z = 0; z < zCount; ++z) {
        const auto& locations = denseLocations[z]->locations;
        auto& tiles = sparseLocations[z]->tiles;
        auto& links = sparseLocations[z]->links;

        size_t tilesCount = eqClasses[z].size();
        linkStatus[z].resize(tilesCount, false);

        absl::flat_hash_map<uint32_t, uint32_t> class2code;
        class2code.reserve(tilesCount);

        for (uint32_t tileCode = 0; tileCode < tilesCount; ++tileCode) {
            uint32_t eqClass = eqClasses[z][tileCode];

            // if parent is copied then all subtree is also copied
            if (z > 0 && linkStatus[z - 1][parent(tileCode)]) {
                linkStatus[z][tileCode] = true;
                continue;
            }

            auto [it, inserted] = class2code.try_emplace(eqClass, tileCode);
            if (!inserted) {
                linkStatus[z][tileCode] = true;
                links.emplace_back(tileCode, it->second);
            } else {
                linkStatus[z][tileCode] = false;
                tiles.emplace_back(tileCode, locations[tileCode]);
            }
        }
    }

    return result;
}

fbs::SparseBlockT sparseToDense(const fbs::SparseBlockT& block)
{
    uint32_t zCount = block.sparse_locations.size();

    WT_ASSERT(block.block_type == fbs::BlockType_Sparse);
    WT_ASSERT(!block.sparse_locations.empty());
    for (const auto& sparseLocations : block.sparse_locations) {
        WT_ASSERT(sparseLocations);
    }

    fbs::SparseBlockT result;
    result.block_type = fbs::BlockType_Dense;
    for (uint32_t z = 0; z < zCount; ++z) {
        result.dense_locations.push_back(std::make_unique<fbs::DenseLocationsT>());
    }

    const auto& sparseLocations = block.sparse_locations;
    const auto& denseLocations = result.dense_locations;

    enum class TileStatus { None, Uniq, Link };
    // z -> (tileCode -> status)
    std::vector<std::vector<TileStatus>> tileStatus(zCount);
    // z -> (tileCode -> tileCode)
    std::vector<std::vector<uint32_t>> linkCodes(zCount);

    for (uint32_t z = 0; z < zCount; ++z) {
        auto& locations = denseLocations[z]->locations;
        const auto& tiles = sparseLocations[z]->tiles;
        const auto& links = sparseLocations[z]->links;

        size_t tilesCount = tilesCountOnZoom(z);
        locations.resize(tilesCount);
        linkCodes[z].resize(tilesCount);
        tileStatus[z].resize(tilesCount, TileStatus::None);

        for (auto tile : tiles) {
            tileStatus[z][tile.tile_code()] = TileStatus::Uniq;
            locations[tile.tile_code()] = tile.location();
        }
        for (auto link : links) {
            tileStatus[z][link.tile_code()] = TileStatus::Link;
            linkCodes[z][link.tile_code()] = link.link_code();
        }
    }

    for (uint32_t z = 0; z < zCount; ++z) {
        auto& locations = denseLocations[z]->locations;

        size_t tilesCount = tilesCountOnZoom(z);

        for (uint32_t tileCode = 0; tileCode < tilesCount; ++tileCode) {
            // no info about tile => parent's subtree was copied
            if (tileStatus[z][tileCode] == TileStatus::None) {
                assert(z > 0);
                auto tileId = decodeTileId(tileCode, z);
                auto parentId = parent(tileId);
                auto parentCode = encodeTileId(parentId);
                assert(tileStatus[z - 1][parentCode] == TileStatus::Link);

                // `parentId` subtree was copied from `newRoot` subtree
                auto newRoot = decodeTileId(linkCodes[z - 1][parentCode], z - 1);

                // make explicit link from `tileId` (in `parentId` subtree) to
                // `newRoot` subtree child with the same relative path
                tileStatus[z][tileCode] = TileStatus::Link;
                linkCodes[z][tileCode] = encodeTileId(changeRoot(tileId, parentId, newRoot));
            }

            // TileStatus::Uniq is already present in result
            if (tileStatus[z][tileCode] == TileStatus::Link) {
                locations[tileCode] = locations[linkCodes[z][tileCode]];
            }
        }
    }

    return result;
}

namespace {

fbs::Location querySparseLocations(const auto* sparseLocations, TileId tileId)
{
    auto findTile = [=](TileId tileId) -> const fbs::LocationItem* {
        return sparseLocations->Get(tileId.z)->tiles()->LookupByKey(encodeTileId(tileId));
    };
    auto findLink = [=](TileId tileId) -> const fbs::LinkItem* {
        return sparseLocations->Get(tileId.z)->links()->LookupByKey(encodeTileId(tileId));
    };
    auto resolveLink = [=](TileId tileId) -> TileId {
        return decodeTileId(findLink(tileId)->link_code(), tileId.z);
    };

    while (!findTile(tileId)) {
        TileId pTileId = tileId;
        while (!findTile(pTileId) && !findLink(pTileId)) {
            pTileId = parent(pTileId);
        }
        if (!findTile(pTileId)) {
            tileId = changeRoot(tileId, pTileId, resolveLink(pTileId));
        }
    }

    return findTile(tileId)->location();
}

fbs::Location queryDenseLocations(const auto* denseLocations, TileId tileId)
{
    return *denseLocations->Get(tileId.z)->locations()->Get(encodeTileId(tileId));
}

} // namespace

fbs::Location queryBlock(const fbs::SparseBlock* block, TileId tileId)
{
    switch (block->block_type()) {
        case fbs::BlockType_Dense:
            return queryDenseLocations(block->dense_locations(), tileId);
        case fbs::BlockType_Sparse:
            return querySparseLocations(block->sparse_locations(), tileId);
        default:
            throw InvalidDatasetError("Invalid block type " + std::to_string(block->block_type()));
    }
}

} // namespace webtiles::index
