#pragma once

#include "webtiles/writer.h"

#include <string>
#include <string_view>

namespace webtiles {

inline std::string AbslUnparseFlag(IndexFormat format)
{
    switch (format) {
        case IndexFormat::BasicPlain:
            return "basic";
        case IndexFormat::Plain:
            return "plain";
        case IndexFormat::Sparse:
            return "sparse";
    }
}

inline bool AbslParseFlag(std::string_view text, IndexFormat* format, std::string* /*error*/)
{
    if (text == "basic") {
        *format = IndexFormat::BasicPlain;
        return true;
    }
    if (text == "plain") {
        *format = IndexFormat::Plain;
        return true;
    }
    if (text == "sparse") {
        *format = IndexFormat::Sparse;
        return true;
    }
    return false;
}

} // namespace webtiles
