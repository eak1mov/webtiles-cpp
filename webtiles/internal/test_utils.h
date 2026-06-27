#pragma once

#include "webtiles/common.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace webtiles::internal::test_utils {

inline const std::filesystem::path TEMP_PATH = "/dev/shm";
inline const std::filesystem::path TESTDATA_PATH = "testdata";

struct TempFile {
    std::string path = TEMP_PATH / "webtiles_test_tempfile";
    ~TempFile() { std::filesystem::remove(path); }
};

inline FileAccess makeFileAccess(const std::string& filePath)
{
    std::ifstream fileStream(filePath, std::ios::binary | std::ios::ate);
    size_t fileSize = fileStream.tellg();
    auto fileData = std::make_shared<std::string>(fileSize, 0);
    fileStream.seekg(0);
    fileStream.read(fileData->data(), fileData->size());

    return [fileData](PackedLocation location) {
        return SharedBuffer::nonOwning(
            std::string_view{*fileData}.substr(location.offset, location.size));
    };
}

} // namespace webtiles::internal::test_utils
