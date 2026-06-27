#include "webtiles/internal/index_format.h"
#include "webtiles/internal/tileindex.h"
#include "webtiles/writer.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/initialize.h"

#include <fstream>
#include <string>

namespace wt = webtiles;
namespace ti = webtiles::internal::tileindex;

ABSL_FLAG(std::string, i, "", "Input file path");
ABSL_FLAG(std::string, o, "", "Output file path");
ABSL_FLAG(wt::IndexFormat, f, wt::IndexFormat::Sparse, "Index format");

int main(int argc, char** argv)
{
    absl::ParseCommandLine(argc, argv);
    absl::InitializeLog();

    std::string inputPath = absl::GetFlag(FLAGS_i);
    std::string outputPath = absl::GetFlag(FLAGS_o);

    auto indexItems = ti::readIndexItems(inputPath);

    auto writer = wt::createWriter(wt::WriterParams{
        .filePath = outputPath,
        .extendedHeader = "extended_header",
        .indexFormat = absl::GetFlag(FLAGS_f),
    });

    for (const auto& item : indexItems) {
        writer->writeTile(
            {.x = item.x, .y = item.y, .z = item.z},
            std::to_string(item.offset) /*data*/,
            std::to_string(item.offset) /*hash*/);
    }

    writer->finalize();

    return 0;
}
