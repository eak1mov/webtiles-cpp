#include "webtiles/internal/tileindex.h"
#include "webtiles/reader.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/initialize.h"

#include <fstream>
#include <string>
#include <vector>

namespace wt = webtiles;
namespace ti = webtiles::internal::tileindex;

ABSL_FLAG(std::string, input_path, "", "Input file path");
ABSL_FLAG(std::string, output_path, "", "Output file path");

int main(int argc, char** argv)
{
    absl::ParseCommandLine(argc, argv);
    absl::InitializeLog();

    std::string inputPath = absl::GetFlag(FLAGS_input_path);
    std::string outputPath = absl::GetFlag(FLAGS_output_path);

    auto reader = wt::createFileReader(inputPath);

    std::vector<ti::IndexItem> indexItems;
    for (auto [tileId, tileLocation] : reader->allTileLocations()) {
        indexItems.push_back({
            .x = tileId.x,
            .y = tileId.y,
            .z = tileId.z,
            .size = static_cast<uint32_t>(tileLocation.size),
            .offset = tileLocation.offset,
        });
    }

    std::ofstream ostream{outputPath, std::ios::binary};
    ostream.write((char*)indexItems.data(), indexItems.size() * sizeof(ti::IndexItem));
    ostream.close();

    return 0;
}
