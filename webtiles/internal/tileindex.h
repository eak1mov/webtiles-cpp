#pragma once

#include <stdint.h>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace webtiles::internal::tileindex {

struct __attribute__((packed)) IndexItem {
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t size;
    uint64_t offset;
};
static_assert(sizeof(IndexItem) == 24);

inline std::vector<IndexItem> readIndexItems(const std::string& filePath)
{
    std::ifstream fileStream(filePath, std::ios::binary | std::ios::ate);
    size_t fileSize = fileStream.tellg();
    std::vector<IndexItem> result(fileSize / sizeof(IndexItem));
    fileStream.seekg(0);
    fileStream.read((char*)result.data(), result.size() * sizeof(IndexItem));
    return result;
}

} // namespace webtiles::internal::tileindex
