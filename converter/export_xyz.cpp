#include "webtiles/reader.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "fmt/format.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace wt = webtiles;

ABSL_FLAG(std::string, input_path, "", "Input file path");
ABSL_FLAG(std::string, output_path, "out/{z}/{x}/{y}.png", "Output file path template");

int main(int argc, char** argv)
{
    absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
    absl::ParseCommandLine(argc, argv);
    absl::InitializeLog();

    std::string inputPath = absl::GetFlag(FLAGS_input_path);
    std::string outputPath = absl::GetFlag(FLAGS_output_path);
    CHECK(!inputPath.empty() && std::filesystem::exists(inputPath));

    LOG(INFO) << "Creating reader...";

    auto reader = wt::createFileReader(inputPath);

    LOG(INFO) << "Writing tiles...";

    for (const auto& [tileId, tileData] : reader->allTiles()) {
        std::string tilePath = fmt::format(
            fmt::runtime(outputPath),
            fmt::arg("x", tileId.x),
            fmt::arg("y", tileId.y),
            fmt::arg("z", tileId.z));
        std::filesystem::create_directories(std::filesystem::path{tilePath}.parent_path());

        std::ofstream ostream{tilePath};
        ostream.write(tileData.data(), tileData.size());
        ostream.close();

        LOG_EVERY_N_SEC(INFO, 10) << "Processed " << COUNTER << " tiles";
    }

    LOG(INFO) << "Done!";

    return 0;
}
