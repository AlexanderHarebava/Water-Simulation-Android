
#pragma once
#include <cstdint>

enum Solver { CG, PCG };
enum Advection { SEMI_LAGRANGIAN, MACCORMACK };

namespace Config {
    extern std::uint16_t N;
    extern std::uint16_t dim;
    extern double dt;
    extern Solver solver;
    extern Advection advection;
    extern std::uint16_t width;
    extern std::uint16_t height;
}

void initConfig(std::uint16_t gridSize);
