#pragma once

#include "webtiles/common.h"
#include "webtiles/internal/iterators.h"

#include <string>

namespace webtiles {

class Reader {
public:
    virtual ~Reader() = default;

    virtual SharedBuffer readMetadata() const = 0;
    virtual std::string extendedHeader() const = 0;

    virtual SharedBuffer readTileData(TileId tileId) const = 0;
    virtual PackedLocation readTileLocation(TileId tileId) const = 0;

    // Usage: for (const auto& [tileId, tileData] : reader->allTiles()) { ... }
    virtual internal::TilesRange allTiles() const = 0;
    virtual internal::TileLocationsRange allTileLocations() const = 0;
};

std::unique_ptr<Reader> createReader(const FileAccess& fileAccess);

std::unique_ptr<Reader> createFileReader(const std::string& filePath);

} // namespace webtiles
