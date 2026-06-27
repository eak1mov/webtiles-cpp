#pragma once

#include <stdint.h>
#include <span>
#include <string>
#include <string_view>

namespace webtiles::internal {

std::string computeHash(std::span<const uint8_t> data);
std::string computeHash(std::string_view data);

} // namespace webtiles::internal
