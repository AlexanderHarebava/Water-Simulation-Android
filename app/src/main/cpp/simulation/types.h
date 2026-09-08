
#pragma once
#include <cstdint>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

enum CellLabel : std::uint8_t {
    EMPTY        = (1 << 0),
    LIQUID       = (1 << 1),
    SOLID        = (1 << 2),
    EXTRAPOLATED = (1 << 3),
};

constexpr inline CellLabel operator|(CellLabel a, CellLabel b) {
return static_cast<CellLabel>(static_cast<int>(a) | static_cast<int>(b));
}

enum RenderMode { TRIANGLES, LINES };
