#pragma once

#include "webtiles/common.h"

#define WT_ASSERT(cond)                                                                            \
    do {                                                                                           \
        if (!(cond)) [[unlikely]] {                                                                \
            throw webtiles::AssertionError("Assertion failed: " #cond);                            \
        }                                                                                          \
    } while (false)
