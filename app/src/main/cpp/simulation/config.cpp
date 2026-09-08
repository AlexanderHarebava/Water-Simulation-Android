
#include "config.h"
#include <algorithm>

namespace Config {
    std::uint16_t N = 32;
    std::uint16_t dim = 3;

    double dt = 0.0000025;

    Solver solver = PCG;

    Advection advection = SEMI_LAGRANGIAN;

    std::uint16_t width = 1080;
    std::uint16_t height = 1920;
}

void initConfig(std::uint16_t gridSize) {
    Config::N = std::clamp<std::uint16_t>(gridSize, 16, 128);
    Config::dim = 3;
}
