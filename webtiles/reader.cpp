#include "webtiles/reader.h"

#include "webtiles/fbs/header_generated.h"
#include "webtiles/index/index.h"

#include <fstream>
#include <memory>

namespace webtiles {

namespace {

class ReaderImpl : public Reader {
public:
    explicit ReaderImpl(const FileAccess& fileAccess);

    SharedBuffer readMetadata() const final;
    std::string extendedHeader() const final { return extendedHeader_; }

    SharedBuffer readTileData(TileId tileId) const final;
    PackedLocation readTileLocation(TileId tileId) const final;

    internal::TilesRange allTiles() const final;
    internal::TileLocationsRange allTileLocations() const final;

private:
    FileAccess fileAccess_;
    fbs::Header header_;
    std::string extendedHeader_;
};

SharedBuffer ReaderImpl::readMetadata() const
{
    const fbs::FileHeader& fileHeader = header_.file_header();
    return fileAccess_({fileHeader.metadata_offset(), fileHeader.metadata_size()});
}

ReaderImpl::ReaderImpl(const FileAccess& fileAccess)
    : fileAccess_{fileAccess}
{
    auto headerData = fileAccess_({0, fbs::HeaderSize_Extended});

    static_assert(sizeof(header_) == fbs::HeaderSize_Regular);
    memcpy(&header_, headerData.data(), sizeof(header_));

    const fbs::FileHeader& fileHeader = header_.file_header();
    if (fileHeader.signature() != fbs::HeaderSignature_Value) {
        throw InvalidFileFormatError("Invalid signature");
    }
    if (fileHeader.version() != fbs::HeaderVersion_MAX) {
        throw InvalidVersionError("Invalid version " + std::to_string(fileHeader.version()));
    }

    extendedHeader_ = std::string_view{headerData}.substr(
        fileHeader.extended_offset(), fileHeader.extended_size());
}

SharedBuffer ReaderImpl::readTileData(TileId tileId) const
{
    return fileAccess_(readTileLocation(tileId));
}

PackedLocation ReaderImpl::readTileLocation(TileId tileId) const
{
    if (tileId.z >= 32) {
        throw InvalidRequestError("Invalid zoom=" + std::to_string(tileId.z));
    }
    if (tileId.x >= (1U << tileId.z)) {
        throw InvalidRequestError("Invalid x=" + std::to_string(tileId.x));
    }
    if (tileId.y >= (1U << tileId.z)) {
        throw InvalidRequestError("Invalid y=" + std::to_string(tileId.y));
    }

    if (tileId.z > header_.index_header().max_zoom()) {
        return {.offset = 0, .size = 0};
    }

    auto indexAccess = [this](PackedLocation location) {
        location.offset += header_.file_header().index_offset();
        return fileAccess_(location);
    };

    PackedLocation tileLocation = index::queryIndex(header_.index_header(), indexAccess, tileId);
    tileLocation.offset += header_.file_header().data_offset();
    return tileLocation;
}

internal::TilesRange ReaderImpl::allTiles() const
{
    return internal::tilesRange(allTileLocations(), fileAccess_);
}

internal::TileLocationsRange ReaderImpl::allTileLocations() const
{
    auto index = index::readIndex(
        header_.index_header(),
        fileAccess_({
            .offset = header_.file_header().index_offset(),
            .size = header_.file_header().index_size(),
        }));
    for (auto& [tileId, tileLocation] : index) {
        tileLocation.offset += header_.file_header().data_offset();
    }
    return index;
}

} // namespace

std::unique_ptr<Reader> createReader(const FileAccess& fileAccess)
{
    return std::make_unique<ReaderImpl>(fileAccess);
}

// TODO(eak1mov): add io error handling or remove function
std::unique_ptr<Reader> createFileReader(const std::string& filePath)
{
    auto fstream = std::make_shared<std::ifstream>(filePath, std::ios::binary);
    return createReader([fstream](PackedLocation location) {
        auto data = std::make_shared<std::string>(static_cast<size_t>(location.size), 0);
        fstream->seekg(location.offset);
        fstream->read(data->data(), data->size());
        return SharedBuffer{data};
    });
}

} // namespace webtiles
