#include "webtiles/internal/tileindex.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/initialize.h"

#include <fstream>
#include <string>
#include <vector>

namespace ti = webtiles::internal::tileindex;

ABSL_FLAG(std::string, t, "", "Test name");
ABSL_FLAG(std::string, o, "", "Output file path");

int main(int argc, char** argv)
{
    absl::ParseCommandLine(argc, argv);
    absl::InitializeLog();

    std::string testName = absl::GetFlag(FLAGS_t);
    std::string outputPath = absl::GetFlag(FLAGS_o);

    std::vector<ti::IndexItem> indexItems;

    if (testName == "data42") {
        indexItems.push_back({.x = 0, .y = 0, .z = 0, .size = 1, .offset = 42});
    } else if (testName == "empty") {
        // do nothing
    } else if (testName == "full5") {
        for (uint32_t z = 0; z < 5; ++z) {
            for (uint32_t x = 0; x < (1U << z); ++x) {
                for (uint32_t y = 0; y < (1U << z); ++y) {
                    indexItems.push_back(
                        {.x = x, .y = y, .z = z, .size = 1, .offset = indexItems.size()});
                }
            }
        }
    } else if (testName == "z20") {
        uint32_t z = 20;
        indexItems.push_back({.x = 0, .y = 0, .z = z, .size = 1, .offset = 0});
        indexItems.push_back(
            {.x = (1U << z) - 1, .y = (1U << z) - 1, .z = z, .size = 1, .offset = 1});
    }

    std::ofstream ostream{outputPath, std::ios::binary};
    ostream.write((char*)indexItems.data(), indexItems.size() * sizeof(ti::IndexItem));
    ostream.close();

    return 0;
}
