#include "webtiles/writer.h"

#include "webtiles/fbs/header_generated.h"
#include "webtiles/index/index.h"
#include "webtiles/internal/asserts.h"
#include "webtiles/internal/digest.h"

#include "absl/container/flat_hash_map.h"

#include <algorithm>
#include <fstream>
#include <numeric>

namespace webtiles {

namespace {

constexpr Zoom MAX_ZOOM = 24;

ZoomLevels calcBlockLevels(Zoom maxZoom)
{
    if (maxZoom <= 8) {
        return {0, maxZoom + 1};
    }
    if (maxZoom <= 15) {
        // 9 -> [4]
        // 10..11 -> [5]
        // 12..13 -> [6]
        // 14..15 -> [7]
        return {0, maxZoom / 2, maxZoom + 1};
    }
    // 16..17 -> [5, 10]
    // 18..20 -> [6, 12]
    // 21..24 -> [7, 14]
    return {0, maxZoom / 3, maxZoom / 3 * 2, maxZoom + 1};
}

uint64_t blockLevelsToMask(const ZoomLevels& blockLevels)
{
    uint64_t result = 0;
    for (Zoom zoom : blockLevels) {
        result |= (1ULL << zoom);
    }
    return result;
}

void writeExtendedHeader(std::ostream& ostream, fbs::Header& header, const std::string& data)
{
    WT_ASSERT(data.size() <= (fbs::HeaderSize_Extended - fbs::HeaderSize_Regular));

    fbs::FileHeader& fileHeader = header.mutable_file_header();
    fileHeader.mutate_extended_offset(ostream.tellp());
    ostream.write(data.data(), data.size());
    fileHeader.mutate_extended_size(uint64_t(ostream.tellp()) - fileHeader.extended_offset());
}

void writeMetadata(std::ostream& ostream, fbs::Header& header, const std::string& metadata)
{
    fbs::FileHeader& fileHeader = header.mutable_file_header();
    fileHeader.mutate_metadata_offset(ostream.tellp());
    ostream.write(metadata.data(), metadata.size());
    fileHeader.mutate_metadata_size(uint64_t(ostream.tellp()) - fileHeader.metadata_offset());
}

void writeIndex(
    std::ostream& ostream,
    fbs::Header& header,
    const WriterParams& writerParams,
    index::IndexMap indexMap)
{
    Zoom maxZoom = 0;
    for (const auto& [tileId, tileLocation] : indexMap) {
        maxZoom = std::max(maxZoom, tileId.z);
    }
    WT_ASSERT(maxZoom <= MAX_ZOOM);

    ZoomLevels blockLevels;
    if (writerParams.indexFormat == IndexFormat::BasicPlain) {
        blockLevels = ZoomLevels(maxZoom + 2);
        std::iota(blockLevels.begin(), blockLevels.end(), 0);
    } else {
        blockLevels = calcBlockLevels(maxZoom);
    }

    fbs::IndexHeader& indexHeader = header.mutable_index_header();
    indexHeader.mutate_magic(fbs::IndexMagic::IndexMagic_Value);
    static_assert(
        static_cast<fbs::IndexFormat>(IndexFormat::BasicPlain) == fbs::IndexFormat_BasicPlain);
    static_assert(static_cast<fbs::IndexFormat>(IndexFormat::Plain) == fbs::IndexFormat_Plain);
    static_assert(static_cast<fbs::IndexFormat>(IndexFormat::Sparse) == fbs::IndexFormat_Sparse);
    indexHeader.mutate_format(static_cast<fbs::IndexFormat>(writerParams.indexFormat));
    indexHeader.mutate_max_zoom(maxZoom);
    indexHeader.mutate_block_levels_mask(blockLevelsToMask(blockLevels));

    index::IndexData indexData = index::writeIndex(indexHeader, std::move(indexMap));
    indexHeader.mutate_root_offset(indexData.rootLocation.offset);
    indexHeader.mutate_root_size(indexData.rootLocation.size);

    fbs::FileHeader& fileHeader = header.mutable_file_header();
    fileHeader.mutate_index_offset(ostream.tellp());
    ostream.write(indexData.data.data(), indexData.data.size());
    fileHeader.mutate_index_size(uint64_t(ostream.tellp()) - fileHeader.index_offset());
}

class WriterImpl : public Writer {
public:
    explicit WriterImpl(WriterParams writerParams);

    void writeTile(TileId tileId, std::string_view tileData) final;
    void writeTile(TileId tileId, std::string_view tileData, std::string_view tileDataHash) final;

    void finalize() final;

private:
    WriterParams writerParams_;
    std::ofstream ostream_;
    fbs::Header header_;
    index::IndexMap indexMap_;
    absl::flat_hash_map<std::string /*tileDataHash*/, PackedLocation> tiles_;
};

WriterImpl::WriterImpl(WriterParams writerParams)
    : writerParams_{std::move(writerParams)}
    , ostream_{writerParams_.filePath, std::ios::binary}
{
    fbs::FileHeader& fileHeader = header_.mutable_file_header();

    fileHeader.mutate_signature(fbs::HeaderSignature_Value);
    fileHeader.mutate_version(fbs::HeaderVersion_V02);

    // reserve memory for header section (will be overwritten in finalize)
    ostream_.write(std::string(fbs::HeaderSize_Extended, 0).data(), fbs::HeaderSize_Extended);

    static_assert(sizeof(header_) == fbs::HeaderSize_Regular);
    ostream_.seekp(fbs::HeaderSize_Regular);

    writeExtendedHeader(ostream_, header_, writerParams_.extendedHeader);
    ostream_.seekp(fbs::HeaderSize_Extended);

    writeMetadata(ostream_, header_, writerParams_.metadata);

    fileHeader.mutate_data_offset(ostream_.tellp());
}

void WriterImpl::writeTile(TileId tileId, std::string_view tileData)
{
    writeTile(tileId, tileData, internal::computeHash(tileData));
}

void WriterImpl::writeTile(TileId tileId, std::string_view tileData, std::string_view tileDataHash)
{
    WT_ASSERT(ostream_);

    if (!tileData.size()) {
        return; // use same location for all missing/empty tiles
    }

    PackedLocation tileLocation{};
    if (auto it = tiles_.find(tileDataHash); it != tiles_.end()) {
        tileLocation = it->second;
    } else {
        tileLocation = {
            .offset = uint64_t(ostream_.tellp()) - header_.file_header().data_offset(),
            .size = tileData.size(),
        };
        tiles_[tileDataHash] = tileLocation;
        ostream_.write(tileData.data(), tileData.size());
    }

    indexMap_.emplace(tileId, tileLocation);
}

void WriterImpl::finalize()
{
    WT_ASSERT(ostream_);

    fbs::FileHeader& fileHeader = header_.mutable_file_header();
    fileHeader.mutate_data_size(uint64_t(ostream_.tellp()) - fileHeader.data_offset());

    writeIndex(ostream_, header_, writerParams_, std::move(indexMap_));

    // rewrite header with updated locations
    ostream_.seekp(0);
    ostream_.write((char*)&header_, sizeof(header_));

    ostream_.close();
}

} // namespace

std::unique_ptr<Writer> createWriter(const WriterParams& writerParams)
{
    return std::make_unique<WriterImpl>(writerParams);
}

} // namespace webtiles
