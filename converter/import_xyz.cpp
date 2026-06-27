#include "webtiles/fbs/response_params_generated.h"
#include "webtiles/internal/index_format.h"
#include "webtiles/writer.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_replace.h"
#include "fmt/format.h"
#include "re2/re2.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>

namespace wt = webtiles;

ABSL_FLAG(std::string, input_path, "", "Input file template");
ABSL_FLAG(std::string, output_path, "tiles.wtiles", "Output file path");
ABSL_FLAG(std::string, metadata_path, "", "Metadata file path (optional)");
ABSL_FLAG(std::string, content_type, "", "Content-Type HTTP header for tile data");
ABSL_FLAG(std::string, content_encoding, "", "Content-Encoding HTTP header for tile data");
ABSL_FLAG(wt::IndexFormat, index_format, wt::IndexFormat::Sparse, "Index format");

std::filesystem::path findRoot(const std::string& pattern)
{
    using namespace fmt::literals;

    std::filesystem::path path0 =
        fmt::format(fmt::runtime(pattern), "x"_a = 0, "y"_a = 0, "z"_a = 0);

    std::filesystem::path path1 =
        fmt::format(fmt::runtime(pattern), "x"_a = 1, "y"_a = 1, "z"_a = 1);

    while (path0 != path1) {
        path0 = path0.parent_path();
        path1 = path1.parent_path();
    }

    return path0;
}

int main(int argc, char** argv)
{
    absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
    absl::ParseCommandLine(argc, argv);
    absl::InitializeLog();

    std::string inputPath = absl::GetFlag(FLAGS_input_path);
    std::string outputPath = absl::GetFlag(FLAGS_output_path);
    std::string metadataPath = absl::GetFlag(FLAGS_metadata_path);
    CHECK(!inputPath.empty());
    CHECK(!outputPath.empty() && !std::filesystem::exists(outputPath));

    CHECK(absl::StrContains(inputPath, "{x}"));
    CHECK(absl::StrContains(inputPath, "{y}"));
    CHECK(absl::StrContains(inputPath, "{z}"));
    std::filesystem::path parentPath = findRoot(inputPath);

    RE2 pathRegex(absl::StrReplaceAll(
        inputPath,
        {
            {"{x}", "(?<x>\\d+)"},
            {"{y}", "(?<y>\\d+)"},
            {"{z}", "(?<z>\\d+)"},
        }));
    CHECK(pathRegex.ok()) << pathRegex.error();
    const auto& pathRegexGroups = pathRegex.NamedCapturingGroups();

    std::string metadata;
    if (!metadataPath.empty()) {
        std::ifstream metadataStream{metadataPath, std::ios::binary};
        metadata = {std::istreambuf_iterator<char>(metadataStream), {}};
    }

    wt::fbs::ResponseParamsT params = {
        .content_type = absl::GetFlag(FLAGS_content_type),
        .content_encoding = absl::GetFlag(FLAGS_content_encoding),
    };
    flatbuffers::FlatBufferBuilder fbb;
    fbb.Finish(wt::fbs::CreateResponseParams(fbb, &params));
    std::string extendedHeader((char*)fbb.GetBufferPointer(), fbb.GetSize());

    auto writer = wt::createWriter(wt::WriterParams{
        .filePath = outputPath,
        .metadata = metadata,
        .extendedHeader = extendedHeader,
        .indexFormat = absl::GetFlag(FLAGS_index_format),
    });

    LOG(INFO) << "Writing tiles...";

    for (const auto& entry : std::filesystem::recursive_directory_iterator(parentPath)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        wt::TileId tileId{};
        std::array<RE2::Arg, 3> args;
        args[pathRegexGroups.at("x") - 1] = RE2::Arg(&tileId.x);
        args[pathRegexGroups.at("y") - 1] = RE2::Arg(&tileId.y);
        args[pathRegexGroups.at("z") - 1] = RE2::Arg(&tileId.z);
        CHECK(RE2::FullMatch(entry.path().string(), pathRegex, args[0], args[1], args[2]));

        std::ifstream fileStream{entry.path(), std::ios::binary};
        std::string tileData{std::istreambuf_iterator<char>(fileStream), {}};

        writer->writeTile(tileId, tileData);

        LOG_EVERY_N_SEC(INFO, 10) << "Processed " << COUNTER << " tiles";
    }

    LOG(INFO) << "Writing index...";

    writer->finalize();

    LOG(INFO) << "Done!";

    return 0;
}
