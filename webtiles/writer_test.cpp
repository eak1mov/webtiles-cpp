#include "webtiles/common.h"
#include "webtiles/internal/test_utils.h"
#include "webtiles/internal/tileindex.h"
#include "webtiles/reader.h"
#include "webtiles/writer.h"

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

void runTest(const std::string& testName, wt::IndexFormat indexFormat)
{
    std::string indexFilePath = tu::TESTDATA_PATH / (testName + ".index");
    auto indexItems = ti::readIndexItems(indexFilePath);

    std::vector<std::pair<wt::TileId, std::string>> inputTiles;
    for (const auto& item : indexItems) {
        inputTiles.push_back({{item.x, item.y, item.z}, std::to_string(item.offset)});
    }

    tu::TempFile tempFile;
    wt::WriterParams writerParams = {
        .filePath = tempFile.path,
        .metadata = "metadata",
        .extendedHeader = "extended_header",
        .indexFormat = indexFormat,
    };

    auto writer = wt::createWriter(writerParams);
    for (const auto& [tileId, tileData] : inputTiles) {
        writer->writeTile(tileId, tileData);
    }
    writer->finalize();

    auto reader = wt::createReader(tu::makeFileAccess(tempFile.path));

    ASSERT_EQ(writerParams.extendedHeader, reader->extendedHeader());
    ASSERT_EQ(writerParams.metadata, reader->readMetadata());

    std::vector<std::pair<wt::TileId, std::string>> outputTiles;
    for (const auto& [tileId, tileData] : reader->allTiles()) {
        outputTiles.emplace_back(tileId, tileData);
    }

    std::ranges::sort(inputTiles, {}, [](auto&& t) { return t.first; });
    std::ranges::sort(outputTiles, {}, [](auto&& t) { return t.first; });

    ASSERT_EQ(inputTiles, outputTiles);
}

} // namespace

TEST(WriterTest, EmptyPlain)
{
    runTest("empty", wt::IndexFormat::Plain);
}

TEST(WriterTest, EmptySparse)
{
    runTest("empty", wt::IndexFormat::Sparse);
}

TEST(WriterTest, SmallPlain)
{
    runTest("small", wt::IndexFormat::Plain);
}

TEST(WriterTest, SmallSparse)
{
    runTest("small", wt::IndexFormat::Sparse);
}

TEST(WriterTest, MediumPlain)
{
    runTest("medium", wt::IndexFormat::Plain);
}

TEST(WriterTest, MediumSparse)
{
    runTest("medium", wt::IndexFormat::Sparse);
}

TEST(WriterTest, Full5Plain)
{
    runTest("full5", wt::IndexFormat::Plain);
}

TEST(WriterTest, Full5Sparse)
{
    runTest("full5", wt::IndexFormat::Sparse);
}

TEST(WriterTest, Z20Sparse)
{
    runTest("z20", wt::IndexFormat::Sparse);
}
