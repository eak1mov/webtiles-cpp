#include "webtiles/index/index.h"

#include "webtiles/index/format_plain.h"
#include "webtiles/index/format_sparse.h"
#include "webtiles/internal/asserts.h"

namespace webtiles::index {

namespace {

ZoomLevels blockLevelsFromMask(uint64_t blockLevelsMask)
{
    ZoomLevels result;
    // TODO(eak1mov): use MAX_ZOOM
    for (uint32_t z = 0; z <= 30; ++z) {
        if (blockLevelsMask & (1U << z)) {
            result.push_back(z);
        }
    }
    return result;
}

} // namespace

IndexData writeIndex(const fbs::IndexHeader& header, IndexMap indexMap)
{
    ZoomLevels blockLevels = blockLevelsFromMask(header.block_levels_mask());

    if (blockLevels.empty()) {
        return {};
    }

    if (header.format() == fbs::IndexFormat_BasicPlain) {
        WT_ASSERT(header.block_levels_mask() == (1U << (header.max_zoom() + 2)) - 1);
    }

    switch (header.format()) {
        case fbs::IndexFormat_BasicPlain:
            [[fallthrough]];
        case fbs::IndexFormat_Plain:
            return {.data = writePlain(blockLevels, indexMap)};
        case fbs::IndexFormat_Sparse:
            return writeSparse(blockLevels, std::move(indexMap));
        default:
            throw InvalidDatasetError("Invalid index format");
    }
}

IndexMap readIndex(const fbs::IndexHeader& header, std::string_view indexData)
{
    ZoomLevels blockLevels = blockLevelsFromMask(header.block_levels_mask());
    PackedLocation rootLocation = {.offset = header.root_offset(), .size = header.root_size()};

    if (blockLevels.empty()) {
        return {};
    }

    switch (header.format()) {
        case fbs::IndexFormat_BasicPlain:
            [[fallthrough]];
        case fbs::IndexFormat_Plain:
            return readPlain(blockLevels, indexData);
        case fbs::IndexFormat_Sparse:
            return readSparse(blockLevels, rootLocation, indexData);
        default:
            throw InvalidDatasetError("Invalid index format");
    }
}

PackedLocation queryIndex(
    const fbs::IndexHeader& header, const FileAccess& fileAccess, TileId tileId)
{
    ZoomLevels blockLevels = blockLevelsFromMask(header.block_levels_mask());
    PackedLocation rootLocation = {.offset = header.root_offset(), .size = header.root_size()};

    if (blockLevels.empty()) {
        return {};
    }

    switch (header.format()) {
        case fbs::IndexFormat_BasicPlain:
            [[fallthrough]];
        case fbs::IndexFormat_Plain:
            return queryPlain(blockLevels, fileAccess, tileId);
        case fbs::IndexFormat_Sparse:
            return querySparse(blockLevels, rootLocation, fileAccess, tileId);
        default:
            throw InvalidDatasetError("Invalid index format");
    }
}

} // namespace webtiles::index
