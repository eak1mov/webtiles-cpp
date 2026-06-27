#include "webtiles/common.h"
#include "webtiles/internal/test_utils.h"
#include "webtiles/internal/tileindex.h"
#include "webtiles/reader.h"
#include "webtiles/writer.h"

#include "benchmark/benchmark.h"

namespace wt = webtiles;
namespace tu = webtiles::internal::test_utils;
namespace ti = webtiles::internal::tileindex;

namespace {

std::vector<ti::IndexItem> generateIndexItems(wt::Zoom z)
{
    std::vector<ti::IndexItem> result;
    for (uint32_t x = 0; x < (1U << z); ++x) {
        for (uint32_t y = 0; y < (1U << z); ++y) {
            result.push_back({.x = x, .y = y, .z = z, .size = 1, .offset = result.size()});
        }
    }
    return result;
}

void writeTiles(
    const std::string& filePath,
    wt::IndexFormat format,
    const std::vector<ti::IndexItem>& indexItems)
{
    auto writer = wt::createWriter(wt::WriterParams{
        .filePath = filePath,
        .indexFormat = format,
    });
    for (const auto& item : indexItems) {
        wt::TileId tileId = {.x = item.x, .y = item.y, .z = item.z};
        writer->writeTile(tileId, /*data*/ " ", /*hash*/ std::to_string(item.offset));
    }
    writer->finalize();
}

} // namespace

void BM_WriteTile(benchmark::State& state, wt::IndexFormat format, wt::Zoom z)
{
    ti::IndexItem indexItem = {.x = 0, .y = 0, .z = z, .size = 1, .offset = 0};

    for (auto _ : state) {
        tu::TempFile tempFile;
        writeTiles(tempFile.path, format, {indexItem});
    }
}

void BM_ReadTile(benchmark::State& state, wt::IndexFormat format, wt::Zoom z)
{
    ti::IndexItem indexItem = {.x = 0, .y = 0, .z = z, .size = 1, .offset = 0};

    tu::TempFile tempFile;
    writeTiles(tempFile.path, format, {indexItem});
    wt::FileAccess fileAccess = tu::makeFileAccess(tempFile.path);

    for (auto _ : state) {
        auto reader = wt::createReader(fileAccess);
        for (auto [tileId, tileData] : reader->allTiles()) {
            benchmark::DoNotOptimize(tileId);
            benchmark::DoNotOptimize(tileData);
        }
    }
}

void BM_QueryTile(benchmark::State& state, wt::IndexFormat format, wt::Zoom z)
{
    ti::IndexItem indexItem = {.x = 0, .y = 0, .z = z, .size = 1, .offset = 0};

    tu::TempFile tempFile;
    writeTiles(tempFile.path, format, {indexItem});
    wt::FileAccess fileAccess = tu::makeFileAccess(tempFile.path);

    for (auto _ : state) {
        auto reader = wt::createReader(fileAccess);
        wt::TileId tileId = {.x = indexItem.x, .y = indexItem.y, .z = indexItem.z};
        benchmark::DoNotOptimize(reader->readTileData(tileId));
    }
}

void BM_WriteMany(benchmark::State& state, wt::IndexFormat format, wt::Zoom z)
{
    auto indexItems = generateIndexItems(z);

    for (auto _ : state) {
        tu::TempFile tempFile;
        writeTiles(tempFile.path, format, indexItems);
    }
}

void BM_ReadMany(benchmark::State& state, wt::IndexFormat format, wt::Zoom z)
{
    auto indexItems = generateIndexItems(z);

    tu::TempFile tempFile;
    writeTiles(tempFile.path, format, indexItems);
    wt::FileAccess fileAccess = tu::makeFileAccess(tempFile.path);

    for (auto _ : state) {
        auto reader = wt::createReader(fileAccess);
        for (auto [tileId, tileData] : reader->allTiles()) {
            benchmark::DoNotOptimize(tileId);
            benchmark::DoNotOptimize(tileData);
        }
    }
}

void BM_QueryMany(benchmark::State& state, wt::IndexFormat format, wt::Zoom z)
{
    auto indexItems = generateIndexItems(z);

    tu::TempFile tempFile;
    writeTiles(tempFile.path, format, indexItems);
    wt::FileAccess fileAccess = tu::makeFileAccess(tempFile.path);

    for (auto _ : state) {
        auto reader = wt::createReader(fileAccess);
        for (const auto& item : indexItems) {
            wt::TileId tileId = {.x = item.x, .y = item.y, .z = item.z};
            benchmark::DoNotOptimize(reader->readTileData(tileId));
        }
    }
}

void BM_WriteFromFile(
    benchmark::State& state, const std::string& fileName, wt::IndexFormat format, wt::Zoom z)
{
    auto indexItems = ti::readIndexItems(tu::TESTDATA_PATH / fileName);

    std::erase_if(indexItems, [=](const auto& item) { return item.z > z; });

    for (auto _ : state) {
        tu::TempFile tempFile;
        writeTiles(tempFile.path, format, indexItems);
    }
}

using enum wt::IndexFormat;
namespace bm = benchmark;

BENCHMARK_CAPTURE(BM_WriteTile, plain_13, Plain, 13)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_ReadTile, plain_13, Plain, 13)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_QueryTile, plain_12, Plain, 12)->Unit(bm::kMicrosecond);

BENCHMARK_CAPTURE(BM_WriteMany, plain_10, Plain, 10)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_ReadMany, plain_11, Plain, 11)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_QueryMany, plain_10, Plain, 10)->Unit(bm::kMillisecond);

BENCHMARK_CAPTURE(BM_WriteFromFile, plain_s12, "small.index", Plain, 12)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_WriteFromFile, plain_m12, "medium.index", Plain, 12)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_WriteFromFile, plain_l12, "large.index", Plain, 12)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_WriteFromFile, plain_s15, "small.index", Plain, 15)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_WriteFromFile, plain_m15, "medium.index", Plain, 15)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_WriteFromFile, plain_l15, "large.index", Plain, 15)->Unit(bm::kMillisecond);

BENCHMARK_CAPTURE(BM_WriteTile, sparse_13, Sparse, 13)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_ReadTile, sparse_13, Sparse, 13)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_QueryTile, sparse_12, Sparse, 12)->Unit(bm::kMicrosecond);

BENCHMARK_CAPTURE(BM_WriteMany, sparse_10, Sparse, 10)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_ReadMany, sparse_11, Sparse, 11)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_QueryMany, sparse_10, Sparse, 10)->Unit(bm::kMillisecond);

BENCHMARK_CAPTURE(BM_WriteFromFile, sparse_s12, "small.index", Sparse, 12)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_WriteFromFile, sparse_m12, "medium.index", Sparse, 12)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_WriteFromFile, sparse_l12, "large.index", Sparse, 12)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_WriteFromFile, sparse_s15, "small.index", Sparse, 15)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_WriteFromFile, sparse_m15, "medium.index", Sparse, 15)->Unit(bm::kMillisecond);
BENCHMARK_CAPTURE(BM_WriteFromFile, sparse_l15, "large.index", Sparse, 15)->Unit(bm::kMillisecond);
