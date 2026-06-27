#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace webtiles {

class Error : public std::runtime_error {
    using runtime_error::runtime_error;
};
class AssertionError : public Error {
    using Error::Error;
};
class InvalidDatasetError : public Error {
    using Error::Error;
};
class InvalidRequestError : public Error {
    using Error::Error;
};
class InvalidFileFormatError : public Error {
    using Error::Error;
};
class InvalidVersionError : public Error {
    using Error::Error;
};

struct TileId {
    uint32_t x;
    uint32_t y;
    uint32_t z;

    auto operator<=>(const TileId&) const = default;
};

struct __attribute__((packed)) PackedLocation {
    uint64_t offset : 40; // max 2^40 = 1024 GiB
    uint64_t size : 24; // max 2^24 = 16 MiB

    bool operator==(const PackedLocation&) const = default;
};
static_assert(sizeof(PackedLocation) == sizeof(uint64_t));

using Zoom = uint32_t;
using ZoomLevels = std::vector<Zoom>;

// Provides uniform interface for reading local or remote file data regardless
// of underlying access method (standard I/O, mmap, HTTP Range, in-memory caches
// with custom deleters). Supports both owning and non-owning data access.
class SharedBuffer {
public:
    SharedBuffer(std::shared_ptr<const char[]> data, size_t size)
        : data_{std::move(data)}
        , size_{size}
    { }
    SharedBuffer(std::shared_ptr<const std::string> data)
        : data_{data, data->data()} // aliasing constructor
        , size_{data->size()}
    { }
    SharedBuffer(std::string&& data)
        : SharedBuffer{std::make_shared<const std::string>(std::move(data))}
    { }

    template<typename R = std::string_view>
    static SharedBuffer nonOwning(R buffer)
    {
        // aliasing constructor, without allocations
        return {{std::shared_ptr<const char[]>(), buffer.data()}, buffer.size()};
    }

    const char* data() const { return data_.get(); }
    size_t size() const { return size_; }

    std::string_view view() const { return {data(), size()}; }

    operator std::string_view() const { return view(); }
    bool operator==(std::string_view other) const { return view() == other; }

private:
    std::shared_ptr<const char[]> data_;
    size_t size_;
};

// must ensure that there are no partial reads
using FileAccess = std::function<SharedBuffer(PackedLocation)>;

} // namespace webtiles
