#include "webtiles/index/format_plain.h"
#include "webtiles/index/format_sparse.h"
#include "webtiles/internal/test_utils.h"
#include "webtiles/internal/tileindex.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <vector>

namespace wt = webtiles;
namespace tu = webtiles::internal::test_utils;
namespace ti = webtiles::internal::tileindex;

namespace {

wt::index::IndexMap loadIndex(const std::string& fileName)
{
    wt::index::IndexMap result;
    for (const auto& item : ti::readIndexItems(tu::TESTDATA_PATH / fileName)) {
        result.insert({{item.x, item.y, item.z}, {item.offset, item.size}});
    }
    return result;
}

auto sorted(const wt::index::IndexMap& indexMap)
{
    std::vector<std::pair<wt::TileId, wt::PackedLocation>> result(indexMap.begin(), indexMap.end());
    std::ranges::sort(result, {}, [](const auto& p) { return p.first; });
    return result;
}

} // namespace

TEST(IndexTest, ReadPlainFull5)
{
    wt::index::IndexMap index = loadIndex("full5.index");
    wt::ZoomLevels blockLevels = {0, 2, 6};

    std::string data = wt::index::writePlain(blockLevels, index);
    wt::index::IndexMap copy = wt::index::readPlain(blockLevels, data);

    EXPECT_EQ(sorted(copy), sorted(index));
}

TEST(IndexTest, QueryPlainFull5)
{
    wt::index::IndexMap index = loadIndex("full5.index");
    wt::ZoomLevels blockLevels = {0, 2, 6};

    std::string data = wt::index::writePlain(blockLevels, index);

    auto fileAccess = [&](wt::PackedLocation location) {
        return wt::SharedBuffer::nonOwning(
            std::string_view{data}.substr(location.offset, location.size));
    };

    for (auto [tileId, tileLocation] : index) {
        ASSERT_EQ(tileLocation, wt::index::queryPlain(blockLevels, fileAccess, tileId));
    }
}

TEST(IndexTest, ReadSparseSmall)
{
    wt::index::IndexMap index = loadIndex("small.index");
    wt::ZoomLevels blockLevels = {0, 7, 14};

    auto [data, rootLocation] = wt::index::writeSparse(blockLevels, index);
    wt::index::IndexMap copy = wt::index::readSparse(blockLevels, rootLocation, data);

    EXPECT_EQ(sorted(copy), sorted(index));
}

TEST(IndexTest, QuerySparseSmall)
{
    wt::index::IndexMap index = loadIndex("small.index");
    wt::ZoomLevels blockLevels = {0, 7, 14};

    auto [data, rootLocation] = wt::index::writeSparse(blockLevels, index);

    auto fileAccess = [&](wt::PackedLocation location) {
        return wt::SharedBuffer::nonOwning(
            std::string_view{data}.substr(location.offset, location.size));
    };

    for (auto [tileId, tileLocation] : index) {
        ASSERT_EQ(
            tileLocation, wt::index::querySparse(blockLevels, rootLocation, fileAccess, tileId));
    }
}

// can be slow, release build should be used
TEST(IndexTest, SparseZ20)
{
    wt::ZoomLevels blockLevels = {0, 7, 14, 21};
    wt::TileId tileId = {.x = 0, .y = 0, .z = 20};
    wt::PackedLocation tileLocation = {.offset = 0, .size = 1};
    wt::index::IndexMap index = {{tileId, tileLocation}};

    auto [data, rootLocation] = wt::index::writeSparse(blockLevels, index);

    wt::index::IndexMap copy = wt::index::readSparse(blockLevels, rootLocation, data);
    ASSERT_EQ(sorted(copy), sorted(index));

    auto fileAccess = [&](wt::PackedLocation location) {
        return wt::SharedBuffer::nonOwning(
            std::string_view{data}.substr(location.offset, location.size));
    };

    ASSERT_EQ(tileLocation, wt::index::querySparse(blockLevels, rootLocation, fileAccess, tileId));
}

TEST(IndexTest, SparseEmpty)
{
    wt::ZoomLevels blockLevels = {0, 8, 16};

    auto [data, rootLocation] = wt::index::writeSparse(blockLevels, {});

    wt::index::IndexMap index = wt::index::readSparse(blockLevels, rootLocation, data);
    ASSERT_TRUE(index.empty());

    auto fileAccess = [&](wt::PackedLocation location) {
        return wt::SharedBuffer::nonOwning(
            std::string_view{data}.substr(location.offset, location.size));
    };

    for (wt::Zoom z = 0; z < blockLevels.back(); ++z) {
        ASSERT_EQ(
            wt::PackedLocation(0, 0),
            wt::index::querySparse(blockLevels, rootLocation, fileAccess, wt::TileId(0, 0, z)));
    }
}
