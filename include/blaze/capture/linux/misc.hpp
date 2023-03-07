#pragma once

#include <cstdint>

namespace blaze {

    enum format : std::uint8_t {

        rgb,
        rgba,
        argb,
        bgra,
        yuv420p,
        yuv444p,
        nv12

    };
};