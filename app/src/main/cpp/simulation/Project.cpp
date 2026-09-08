
#include "Project.h"

void Project3D::project()
{
    const std::uint64_t n = _grid.activeCellsNb();
    if (n == 0) return;

    _x.resize(n);
    _b.resize(n);

    preparePressureSolving(_A, _b);
    ConjugateGradient(_A, _x, _b, _grid, _r, _z, _s);

    _grid._pressure.reset();

    const std::uint16_t X = _grid._surface.x();
    const std::uint16_t Y = _grid._surface.y();
    const std::uint16_t Z = _grid._surface.z();


    for (std::uint16_t k = 0; k < Z; ++k)
        for (std::uint16_t j = 0; j < Y; ++j)
            for (std::uint16_t i = 0; i < X; ++i) {
                const std::uint64_t id = _grid._pressureID(i, j, k);
                if (id > 0) _grid._pressure(i, j, k) = _x(id - 1);
            }


    for (std::uint16_t k = 0; k < _grid._U.z(); ++k)
        for (std::uint16_t j = 0; j < _grid._U.y(); ++j)
            for (std::uint16_t i = 0; i < _grid._U.x(); ++i)
                if (_grid._U.label(i, j, k) & LIQUID)
                    _grid._U(i, j, k) -= Config::N *
                                         (_grid._pressure(i, j, k) - _grid._pressure(i - 1, j, k));

    for (std::uint16_t k = 0; k < _grid._V.z(); ++k)
        for (std::uint16_t j = 0; j < _grid._V.y(); ++j)
            for (std::uint16_t i = 0; i < _grid._V.x(); ++i)
                if (_grid._V.label(i, j, k) & LIQUID)
                    _grid._V(i, j, k) -= Config::N *
                                         (_grid._pressure(i, j, k) - _grid._pressure(i, j - 1, k));

    for (std::uint16_t k = 0; k < _grid._W.z(); ++k)
        for (std::uint16_t j = 0; j < _grid._W.y(); ++j)
            for (std::uint16_t i = 0; i < _grid._W.x(); ++i)
                if (_grid._W.label(i, j, k) & LIQUID)
                    _grid._W(i, j, k) -= Config::N *
                                         (_grid._pressure(i, j, k) - _grid._pressure(i, j, k - 1));


    for (std::uint16_t k = 0; k < _grid._U.z(); ++k)
        for (std::uint16_t j = 0; j < _grid._U.y(); ++j)
            for (std::uint16_t i = 0; i < _grid._U.x(); ++i)
                if (_grid._U.label(i, j, k) & SOLID) _grid._U(i, j, k) = 0.0;

    for (std::uint16_t k = 0; k < _grid._V.z(); ++k)
        for (std::uint16_t j = 0; j < _grid._V.y(); ++j)
            for (std::uint16_t i = 0; i < _grid._V.x(); ++i)
                if (_grid._V.label(i, j, k) & SOLID) _grid._V(i, j, k) = 0.0;

    for (std::uint16_t k = 0; k < _grid._W.z(); ++k)
        for (std::uint16_t j = 0; j < _grid._W.y(); ++j)
            for (std::uint16_t i = 0; i < _grid._W.x(); ++i)
                if (_grid._W.label(i, j, k) & SOLID) _grid._W(i, j, k) = 0.0;
}





void Project3D::preparePressureSolving(
        Eigen::SparseMatrix<double>& A,
        Eigen::VectorXd& b)
{
    const std::uint64_t n = _grid.activeCellsNb();
    A.resize(n, n);

    _triplets.clear();
    _triplets.reserve(static_cast<std::size_t>(n) * 7);

    _grid._Adiag.reset();
    _grid._Ax.reset();
    _grid._Ay.reset();
    _grid._Az.reset();
    _grid._precon.reset();

    constexpr double scale = 1.0;
    const std::uint16_t X = _grid._surface.x();
    const std::uint16_t Y = _grid._surface.y();
    const std::uint16_t Z = _grid._surface.z();

    for (std::uint16_t k = 0; k < Z; ++k)
        for (std::uint16_t j = 0; j < Y; ++j)
            for (std::uint16_t i = 0; i < X; ++i)
            {
                const std::uint64_t id = _grid._pressureID(i, j, k);
                if (id == 0) continue;

                const int realID = static_cast<int>(id - 1);
                std::uint64_t neibID = 0;


                if (i > 0 && _grid._pressureID(i - 1, j, k) > 0)
                    _grid._Adiag(i, j, k) += scale;
                if (j > 0 && _grid._pressureID(i, j - 1, k) > 0)
                    _grid._Adiag(i, j, k) += scale;
                if (k > 0 && _grid._pressureID(i, j, k - 1) > 0)
                    _grid._Adiag(i, j, k) += scale;


                if (i + 1 < X && (neibID = _grid._pressureID(i + 1, j, k)) > 0) {
                    const int c = static_cast<int>(neibID - 1);
                    _triplets.emplace_back(realID, c, -scale);
                    _triplets.emplace_back(c, realID, -scale);
                    _grid._Adiag(i, j, k) += scale;
                    _grid._Ax(i, j, k) = -scale;
                } else if (i + 1 < X && _grid._surface.label(i + 1, j, k) & EMPTY) {
                    _grid._Adiag(i, j, k) += scale;
                }

                if (j + 1 < Y && (neibID = _grid._pressureID(i, j + 1, k)) > 0) {
                    const int c = static_cast<int>(neibID - 1);
                    _triplets.emplace_back(realID, c, -scale);
                    _triplets.emplace_back(c, realID, -scale);
                    _grid._Adiag(i, j, k) += scale;
                    _grid._Ay(i, j, k) = -scale;
                } else if (j + 1 < Y && _grid._surface.label(i, j + 1, k) & EMPTY) {
                    _grid._Adiag(i, j, k) += scale;
                }

                if (k + 1 < Z && (neibID = _grid._pressureID(i, j, k + 1)) > 0) {
                    const int c = static_cast<int>(neibID - 1);
                    _triplets.emplace_back(realID, c, -scale);
                    _triplets.emplace_back(c, realID, -scale);
                    _grid._Adiag(i, j, k) += scale;
                    _grid._Az(i, j, k) = -scale;
                } else if (k + 1 < Z && _grid._surface.label(i, j, k + 1) & EMPTY) {
                    _grid._Adiag(i, j, k) += scale;
                }

                std::uint8_t nonSolidNeib = 6;
                if (i == 0) --nonSolidNeib;
                if (j == 0) --nonSolidNeib;
                if (k == 0) --nonSolidNeib;
                if (i == X - 1) --nonSolidNeib;
                if (j == Y - 1) --nonSolidNeib;
                if (k == Z - 1) --nonSolidNeib;

                _triplets.emplace_back(realID, realID, static_cast<double>(nonSolidNeib));
                b(realID) = div(i, j, k);
            }

    A.setFromTriplets(_triplets.begin(), _triplets.end());
}

// 3D Staggered Grid divergence with central difference
inline double Project3D::div(
        const std::uint16_t i,
        const std::uint16_t j,
        const std::uint16_t k
    ) const
{
    const double h = 1.0/Config::N;
    const double Udiv = _grid._U(i+1, j, k) - _grid._U(i, j, k);
    const double Vdiv = _grid._V(i, j+1, k) - _grid._V(i, j, k);
    const double Zdiv = _grid._W(i, j, k+1) - _grid._W(i, j, k);
    return - h * (Udiv + Vdiv + Zdiv);
}

void Project2D::project()
{
    if (_grid.activeCellsNb() > 0)
    {
        const std::uint64_t n = _grid.activeCellsNb();
        _x.resize(n);
        _b.resize(n);
        preparePressureSolving(_A, _b);
        ConjugateGradient(_A, _x, _b, _grid, _r, _z, _s);
        _grid._pressure.reset();
        std::uint64_t id;
        for (std::uint16_t j = 0;
             j < std::max(_grid._U.y(), _grid._V.y());
             ++j)
        {
            for (std::uint16_t i = 0;
                 i < std::max(_grid._U.x(), _grid._V.x());
                 ++i)
            {
                if (j < _grid._surface.y() &&
                    i < _grid._surface.x() &&
                    (id = _grid._pressureID(i, j, 0)) > 0)
                {
                    _grid._pressure(i, j, 0) = _x(id-1);
                }
                if (j < _grid._U.y() &&
                    i < _grid._U.x() &&
                    _grid._U.label(i, j, 0) & LIQUID)
                {
                    _grid._U(i, j, 0) -=
                            Config::N*(_grid._pressure(i, j, 0) -
                                       _grid._pressure(i-1, j, 0));
                }
                if (j < _grid._V.y() &&
                    i < _grid._V.x() &&
                    _grid._V.label(i, j, 0) & LIQUID)
                {
                    _grid._V(i, j, 0) -=
                            Config::N*(_grid._pressure(i, j, 0) -
                                       _grid._pressure(i, j-1, 0));
                }
            }
        }
    }
}

// Prepare the 2D Laplacian matrice (A) with cells inside the liquid
// and compute divergence (b) at thoses cell
// After this we just need to find x from Ax = b
void Project2D::preparePressureSolving(
        Eigen::SparseMatrix<double>& A,
        Eigen::VectorXd& b
    )
{
    A.resize(_grid.activeCellsNb(), _grid.activeCellsNb());
    A.data().squeeze();
    A.reserve(Eigen::VectorXi::Constant(_grid.activeCellsNb(), 5));
    _grid._Adiag.reset();
    _grid._Ax.reset();
    _grid._Ay.reset();
    _grid._Az.reset();
    _grid._precon.reset();

    const double scale = 1;
    for (std::uint64_t n = 0; n < _grid._surface.maxIt(); ++n)
    {
        const std::uint64_t xy =
            static_cast<std::uint64_t>(_grid._surface.x()*_grid._surface.y());
        const std::uint64_t m = n % xy;
        const std::uint16_t i = m % _grid._surface.x();
        const std::uint16_t j = m / _grid._surface.x();
        const std::uint16_t k = 0;

        std::uint64_t id = _grid._pressureID(i, j, k);
        if (id > 0)
        {
            const std::uint64_t realID = id-1;
            std::uint64_t neibID = 0;
            if (i > 0 && (neibID = _grid._pressureID(i-1, j, k)) > 0)
            {
                A.coeffRef(realID, neibID-1) = -scale;
                _grid._Adiag(i, j, k) += scale;
            }
            if (i+1 < _grid._surface.x() &&
                    (neibID = _grid._pressureID(i+1, j, k)) > 0)
            {
                A.coeffRef(realID, neibID-1) = -scale;
                _grid._Adiag(i, j, k) += scale;
                _grid._Ax(i, j, k) = -scale;
            }
            else if (i+1 < _grid._surface.x() &&
                    _grid._surface.label(i+1, j, k) & EMPTY)
            {
                _grid._Adiag(i, j, k) += scale;
            }
            if (j > 0 && (neibID = _grid._pressureID(i, j-1, k)) > 0)
            {
                A.coeffRef(realID, neibID-1) = -scale;
                _grid._Adiag(i, j, k) += scale;
            }
            if (j+1 < _grid._surface.y() &&
                    (neibID = _grid._pressureID(i, j+1, k)) > 0)
            {
                A.coeffRef(realID, neibID-1) = -scale;
                _grid._Adiag(i, j, k) += scale;
                _grid._Ay(i, j, k) = -scale;
            }
            else if (j+1 < _grid._surface.y() &&
                    _grid._surface.label(i, j+1, k) & EMPTY)
            {
                _grid._Adiag(i, j, k) += scale;
            }

            std::uint8_t nonSolidNeib = 4;
            if (i == 0)
                nonSolidNeib--;
            if (j == 0)
                nonSolidNeib--;
            if (i == _grid._surface.x()-1)
                nonSolidNeib--;
            if (j == _grid._surface.y()-1)
                nonSolidNeib--;
            A.coeffRef(realID, realID) = nonSolidNeib;
            b.coeffRef(realID) = div(i, j, k);
        }
    }
    A.makeCompressed();
}

// 2D Staggered Grid divergence with central difference
inline double Project2D::div(
        const std::uint16_t i,
        const std::uint16_t j,
        const std::uint16_t k
    ) const
{
    const double h = 1.0/Config::N;
    const double Udiv = _grid._U(i+1, j, k) - _grid._U(i, j, k);
    const double Vdiv = _grid._V(i, j+1, k) - _grid._V(i, j, k);
    return - h * (Udiv + Vdiv);
}
