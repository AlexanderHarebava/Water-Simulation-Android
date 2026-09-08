
#include "Advect.h"
#include <algorithm>


inline double Advect3D::interp(
        const Field<double, std::uint16_t>& F,
        double x, double y, double z) const
{
    x = std::clamp(x, 0.0, static_cast<double>(F.x() - 1));
    y = std::clamp(y, 0.0, static_cast<double>(F.y() - 1));
    z = std::clamp(z, 0.0, static_cast<double>(F.z() - 1));

    const std::uint16_t i0 = static_cast<std::uint16_t>(x);
    const std::uint16_t j0 = static_cast<std::uint16_t>(y);
    const std::uint16_t k0 = static_cast<std::uint16_t>(z);

    const std::uint16_t i1 = static_cast<std::uint16_t>(
            std::min<int>(i0 + 1, F.x() - 1));
    const std::uint16_t j1 = static_cast<std::uint16_t>(
            std::min<int>(j0 + 1, F.y() - 1));
    const std::uint16_t k1 = static_cast<std::uint16_t>(
            std::min<int>(k0 + 1, F.z() - 1));

    const double s1 = x - i0, s0 = 1.0 - s1;
    const double t1 = y - j0, t0 = 1.0 - t1;
    const double u1 = z - k0, u0 = 1.0 - u1;

    return s0 * ( t0 * ( u0 * F(i0, j0, k0) + u1 * F(i0, j0, k1) )
                  + t1 * ( u0 * F(i0, j1, k0) + u1 * F(i0, j1, k1) ) )
           + s1 * ( t0 * ( u0 * F(i1, j0, k0) + u1 * F(i1, j0, k1) )
                    + t1 * ( u0 * F(i1, j1, k0) + u1 * F(i1, j1, k1) ) );
}

void Advect3D::advect(
        const StaggeredGrid<double, std::uint16_t>& grid,
        Field<double, std::uint16_t>& F,
        Field<double, std::uint16_t>& Fprev,
        const std::uint8_t b)
{
    Fprev = F;
    const double dt = Config::dt * Config::N;
    const std::uint16_t X = grid._surface.x();
    const std::uint16_t Y = grid._surface.y();
    const std::uint16_t Z = grid._surface.z();

    for (std::uint16_t k = 0; k < Z; ++k)
        for (std::uint16_t j = 0; j < Y; ++j)
            for (std::uint16_t i = 0; i < X; ++i)
            {

                if (F.label(i, j, k) & SOLID) continue;

                const double x = static_cast<double>(i) - dt * grid.getU(i, j, k, b);
                const double y = static_cast<double>(j) - dt * grid.getV(i, j, k, b);
                const double z = static_cast<double>(k) - dt * grid.getW(i, j, k, b);

                F(i, j, k) = interp(Fprev, x, y, z);
            }
}
