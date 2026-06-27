#include "webtiles/internal/digest.h"

#include "xxhash.h"

#include <array>
#include <cstdint>

namespace webtiles::internal {

std::string computeHash(std::span<const uint8_t> data)
{
    XXH128_hash_t hash = XXH3_128bits(data.data(), data.size());
    XXH128_canonical_t result;
    XXH128_canonicalFromHash(&result, hash);
    return {reinterpret_cast<char*>(result.digest), sizeof(result.digest)};
}

std::string computeHash(std::string_view data)
{
    return computeHash({reinterpret_cast<const uint8_t*>(data.data()), data.size()});
}

} // namespace webtiles::internal
