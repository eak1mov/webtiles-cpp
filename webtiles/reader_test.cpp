#include "webtiles/internal/test_utils.h"
#include "webtiles/internal/tileindex.h"
#include "webtiles/reader.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace wt = webtiles;
namespace tu = webtiles::internal::test_utils;
namespace ti = webtiles::internal::tileindex;

namespace {

void runTest(const std::string& indexFileName, const std::string& wtFileName)
{
    std::string indexFilePath = tu::TESTDATA_PATH / indexFileName;
    std::string pmFilePath = tu::TESTDATA_PATH / wtFileName;

    std::vector<std::pair<wt::TileId, std::string>> inputTiles;
    for (const auto& item : ti::readIndexItems(indexFilePath)) {
        inputTiles.push_back({{item.x, item.y, item.z}, std::to_string(item.offset)});
    }

    auto reader = wt::createReader(tu::makeFileAccess(pmFilePath));

    ASSERT_NO_THROW(reader->readTileData(wt::TileId(0, 0, 0)));

    for (const auto& [tileId, tileData] : std::views::take(inputTiles, 10'000)) {
        ASSERT_EQ(tileData, reader->readTileData(tileId));
    }

    std::vector<std::pair<wt::TileId, std::string>> outputTiles;
    for (const auto& [tileId, tileData] : reader->allTiles()) {
        outputTiles.emplace_back(tileId, tileData);
    }

    std::ranges::sort(inputTiles, {}, [](auto&& t) { return t.first; });
    std::ranges::sort(outputTiles, {}, [](auto&& t) { return t.first; });

    ASSERT_EQ(inputTiles, outputTiles);
}

} // namespace

TEST(ReaderTest, EmptyBasic)
{
    runTest("empty.index", "empty_basic.wtiles");
}

TEST(ReaderTest, EmptyPlain)
{
    runTest("empty.index", "empty_plain.wtiles");
}

TEST(ReaderTest, EmptySparse)
{
    runTest("empty.index", "empty_sparse.wtiles");
}

TEST(ReaderTest, SmallBasic)
{
    runTest("small.index", "small_plain.wtiles");
}

TEST(ReaderTest, SmallPlain)
{
    runTest("small.index", "small_plain.wtiles");
}

TEST(ReaderTest, SmallSparse)
{
    runTest("small.index", "small_sparse.wtiles");
}

TEST(ReaderTest, MediumBasic)
{
    runTest("medium.index", "medium_basic.wtiles");
}

TEST(ReaderTest, MediumPlain)
{
    runTest("medium.index", "medium_plain.wtiles");
}

TEST(ReaderTest, MediumSparse)
{
    runTest("medium.index", "medium_sparse.wtiles");
}

TEST(ReaderTest, LargeBasic)
{
    runTest("large.index", "large_plain.wtiles");
}

TEST(ReaderTest, LargePlain)
{
    runTest("large.index", "large_plain.wtiles");
}

TEST(ReaderTest, LargeSparse)
{
    runTest("large.index", "large_sparse.wtiles");
}

TEST(ReaderTest, Full5Sparse)
{
    runTest("full5.index", "full5_sparse.wtiles");
}

TEST(ReaderTest, Z20Sparse)
{
    runTest("z20.index", "z20_sparse.wtiles");
}

TEST(ReaderTest, Data42Sparse)
{
    auto reader = wt::createReader(tu::makeFileAccess(tu::TESTDATA_PATH / "data42_sparse.wtiles"));

    ASSERT_EQ(std::string("42"), reader->readTileData(wt::TileId(0, 0, 0)));
}

TEST(ReaderTest, Empty10Sparse)
{
    auto reader = wt::createReader(tu::makeFileAccess(tu::TESTDATA_PATH / "empty_sparse.wtiles"));

    EXPECT_NO_THROW(reader->readTileData(wt::TileId(0, 0, 0)));

    for (uint32_t z = 0; z < 10; ++z) {
        ASSERT_EQ(0u, reader->readTileLocation(wt::TileId(0, 0, z)).size);
    }
}
