
#include "Fluids.h"
#include <utility>
#include <cmath>
#include <algorithm>
#include <android/log.h>

#define TAG_FLUID "FluidSim3D"

Fluids::Fluids() {
    const std::uint64_t size =
            static_cast<std::uint64_t>(_grid._surface.x()) *
            static_cast<std::uint64_t>(_grid._surface.y()) *
            static_cast<std::uint64_t>(_grid._surface.z());

    _texture.resize(size);

    _advection = std::make_unique<Advect3D>();
    _projection = std::make_unique<Project3D>(_grid);

    __android_log_print(
            ANDROID_LOG_INFO,
            TAG_FLUID,
            "Fluids 3D created: %d x %d x %d",
            _grid._surface.x(),
            _grid._surface.y(),
            _grid._surface.z()
    );
}

void Fluids::update(const std::uint64_t iteration) {
    _iteration = iteration;
    step();
    updateTexture3D();
}

void Fluids::setGravity(float gx, float gy, float gz) {
    _gravityX = gx;
    _gravityY = gy;
    _gravityZ = gz;
}



void Fluids::step() {
    if (_iteration == 0 || _waterAmountDirty) {
        initSphere();
        _waterAmountDirty = false;
    }

    _grid._surface.setLabels(_grid._U, _grid._V, _grid._W);

    extrapolate(_grid._U, _grid._UPrev, 2);
    extrapolate(_grid._V, _grid._VPrev, 2);
    extrapolate(_grid._W, _grid._WPrev, 2);

    _advection->advect(_grid, _grid._surface, _grid._surfacePrev, 0);


    for (std::uint16_t k = 0; k < _grid._surface.z(); ++k)
        for (std::uint16_t j = 0; j < _grid._surface.y(); ++j)
            for (std::uint16_t i = 0; i < _grid._surface.x(); ++i) {
                if (!std::isfinite(_grid._surface(i, j, k))) {
                    _grid._surface(i, j, k) = 2.0;
                }
            }

    redistancing(2, _grid._surface, _grid._surfacePrev);
    conserveVolume();
    _advection->advect(_grid, _grid._U, _grid._UPrev, 1);
    _advection->advect(_grid, _grid._V, _grid._VPrev, 2);
    _advection->advect(_grid, _grid._W, _grid._WPrev, 3);

    _grid._surface.setLabels(_grid._U, _grid._V, _grid._W);

    addForces();

    _grid.tagActiveCells();
    if (_grid.activeCellsNb() == 0) {
        __android_log_print(ANDROID_LOG_WARN, TAG_FLUID, "Water lost! Reseeding.");
        initSurface();
        _grid._U.reset(); _grid._V.reset(); _grid._W.reset();
        _grid._surface.setLabels(_grid._U, _grid._V, _grid._W);
        _grid.tagActiveCells();
    }
    _projection->project();

}

void Fluids::initSphere() {
    const double cx = _grid._surface.x() / 2.0;
    const double cy = _grid._surface.y() / 2.0;
    const double cz = _grid._surface.z() / 2.0;

    const double baseRadius = _grid._surface.x() / 2.5;
    const double amountRadius =
            baseRadius * std::cbrt(static_cast<double>(_waterAmount));


    const double maxRadius = _grid._surface.x() * 0.49;
    const double radius = std::min(amountRadius, maxRadius);

    for (std::uint16_t k = 0; k < _grid._surface.z(); ++k) {
        for (std::uint16_t j = 0; j < _grid._surface.y(); ++j) {
            for (std::uint16_t i = 0; i < _grid._surface.x(); ++i) {
                double dx = i - cx;
                double dy = j - cy;
                double dz = k - cz;
                double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

                _grid._surface(i, j, k) = (dist < radius) ? -2.0 : 2.0;
            }
        }
    }

    _grid._U.reset();
    _grid._V.reset();
    _grid._W.reset();
    _grid._pressure.reset();

    _targetVolume = -1.0;
}

void Fluids::initSurface() {

    const double fill = _grid._surface.y() * 0.75;
    for (std::uint16_t k = 0; k < _grid._surface.z(); ++k)
        for (std::uint16_t j = 0; j < _grid._surface.y(); ++j)
            for (std::uint16_t i = 0; i < _grid._surface.x(); ++i)
                _grid._surface(i, j, k) =
                        std::clamp(static_cast<double>(j) - fill, -2.0, 2.0);
    _targetVolume = -1.0;
}


void Fluids::addForces() {
    const double forceScale = 75.0;
    const double damping = _damping;
    const double maxVel = 5000.0;

    for (std::uint16_t k = 0; k < _grid._U.z(); ++k)
        for (std::uint16_t j = 0; j < _grid._U.y(); ++j)
            for (std::uint16_t i = 0; i < _grid._U.x(); ++i) {

                if (_grid._U.label(i, j, k) & LIQUID) {
                    double v = _grid._U(i, j, k) + _gravityX * forceScale;
                    v *= damping;
                    _grid._U(i, j, k) = std::clamp(v, -maxVel, maxVel);
                }
            }

    for (std::uint16_t k = 0; k < _grid._V.z(); ++k)
        for (std::uint16_t j = 0; j < _grid._V.y(); ++j)
            for (std::uint16_t i = 0; i < _grid._V.x(); ++i) {
                if (_grid._V.label(i, j, k) & LIQUID) {
                    double v = _grid._V(i, j, k) + _gravityY * forceScale;
                    v *= damping;
                    _grid._V(i, j, k) = std::clamp(v, -maxVel, maxVel);
                }
            }

    for (std::uint16_t k = 0; k < _grid._W.z(); ++k)
        for (std::uint16_t j = 0; j < _grid._W.y(); ++j)
            for (std::uint16_t i = 0; i < _grid._W.x(); ++i) {
                if (_grid._W.label(i, j, k) & LIQUID) {
                    double v = _grid._W(i, j, k) + _gravityZ * forceScale;
                    v *= damping;
                    _grid._W(i, j, k) = std::clamp(v, -maxVel, maxVel);
                }
            }
}

void Fluids::extrapolate(
        Field<double, std::uint16_t>& F,
        Field<double, std::uint16_t>& Ftemp,
        std::uint16_t nbIte
) const {
    const std::uint16_t X = F.x(), Y = F.y(), Z = F.z();
    std::uint16_t it = 0;
    std::uint64_t changed = 0;
    do {
        changed = 0;
        for (std::uint16_t k = 0; k < Z; ++k)
            for (std::uint16_t j = 0; j < Y; ++j)
                for (std::uint16_t i = 0; i < X; ++i)
                {
                    const CellLabel lbl = F.label(i, j, k);
                    if (lbl & (LIQUID | SOLID | EXTRAPOLATED)) {
                        Ftemp(i, j, k) = F(i, j, k);
                        Ftemp.label(i, j, k) = lbl;
                        continue;
                    }

                    std::uint8_t nbNeighbors = 0;
                    double value = 0.0;
                    if (i + 1 < X && F.checked(i + 1, j, k)) { ++nbNeighbors; value += F(i + 1, j, k); }
                    if (i > 0     && F.checked(i - 1, j, k)) { ++nbNeighbors; value += F(i - 1, j, k); }
                    if (j + 1 < Y && F.checked(i, j + 1, k)) { ++nbNeighbors; value += F(i, j + 1, k); }
                    if (j > 0     && F.checked(i, j - 1, k)) { ++nbNeighbors; value += F(i, j - 1, k); }
                    if (k + 1 < Z && F.checked(i, j, k + 1)) { ++nbNeighbors; value += F(i, j, k + 1); }
                    if (k > 0     && F.checked(i, j, k - 1)) { ++nbNeighbors; value += F(i, j, k - 1); }

                    if (nbNeighbors > 0) {
                        ++changed;
                        Ftemp(i, j, k) = value / static_cast<double>(nbNeighbors);
                        Ftemp.label(i, j, k) = EXTRAPOLATED;
                    } else {
                        Ftemp(i, j, k) = F(i, j, k);
                        Ftemp.label(i, j, k) = lbl;
                    }
                }
        std::swap(F, Ftemp);
        if (nbIte > 0 && ++it == nbIte) {
            return;
        }
    } while (changed > 0);
}

void Fluids::redistancing(
        const std::uint64_t nbIte,
        Field<double, std::uint16_t>& field,
        Field<double, std::uint16_t>& fieldTemp
) {
    const double dx = 1.0 / static_cast<double>(Config::N);
    const std::uint16_t X = field.x(), Y = field.y(), Z = field.z();


    for (std::uint16_t k = 0; k < Z; ++k)
        for (std::uint16_t j = 0; j < Y; ++j)
            for (std::uint16_t i = 0; i < X; ++i)
            {
                const bool nearInterface =
                        (i+1 < X && !((field(i,j,k) >= 0) ^ (field(i+1,j,k) < 0))) ||
                        (i   > 0 && !((field(i,j,k) >= 0) ^ (field(i-1,j,k) < 0))) ||
                        (j+1 < Y && !((field(i,j,k) >= 0) ^ (field(i,j+1,k) < 0))) ||
                        (j   > 0 && !((field(i,j,k) >= 0) ^ (field(i,j-1,k) < 0))) ||
                        (k+1 < Z && !((field(i,j,k) >= 0) ^ (field(i,j,k+1) < 0))) ||
                        (k   > 0 && !((field(i,j,k) >= 0) ^ (field(i,j,k-1) < 0)));
                if (nearInterface) { _rdF(i, j, k) = 1; _rdF.label(i, j, k) = LIQUID; }
                else               { _rdF(i, j, k) = 0; _rdF.label(i, j, k) = EMPTY; }
            }

    extrapolate(_rdF, fieldTemp, 3);


    const double dist = 2.0;
    for (std::uint16_t k = 0; k < Z; ++k)
        for (std::uint16_t j = 0; j < Y; ++j)
            for (std::uint16_t i = 0; i < X; ++i)
            {
                const double sgn = (field(i, j, k) <= 0.0) ? -1.0 : 1.0;
                double q = (_rdF(i, j, k) == 1) ? field(i, j, k) : dist * sgn;
                if (std::abs(q) > dist) q = dist * sgn;
                _rdQ(i, j, k) = q;
            }
    std::swap(field, _rdQ);


    for (std::uint16_t k = 0; k < Z; ++k)
        for (std::uint16_t j = 0; j < Y; ++j)
            for (std::uint16_t i = 0; i < X; ++i)
            {
                const double O0 = field(i, j, k);
                _rdQ(i, j, k) = O0 / std::sqrt(O0 * O0 + 0.25);
            }


    for (std::uint64_t relaxit = 0; relaxit < nbIte; ++relaxit) {
        for (std::uint16_t k = 0; k < Z; ++k)
            for (std::uint16_t j = 0; j < Y; ++j)
                for (std::uint16_t i = 0; i < X; ++i)
                {
                    if (_rdF(i, j, k) == 1) {
                        const double gO = field.gradLength(i, j, k);
                        _rdN(i, j, k) = field(i, j, k) +
                                        0.5 * dx * (-_rdQ(i, j, k) * (gO - 1.0));
                    } else {
                        _rdN(i, j, k) = field(i, j, k);
                    }
                }
        std::swap(field, _rdN);
    }
}

void Fluids::updateTexture3D() {
    std::uint64_t it = 0;
    for (std::uint16_t k = 0; k < _grid._surface.z(); ++k) {
        for (std::uint16_t j = 0; j < _grid._surface.y(); ++j) {
            for (std::uint16_t i = 0; i < _grid._surface.x(); ++i) {
                const double phi = _grid._surface(i, j, k);

                double density = std::clamp(0.5 - phi * 0.5, 0.0, 1.0);
                _texture[it] = static_cast<std::uint8_t>(density * 255.0);
                ++it;
            }
        }
    }
}


double Fluids::conserveVolume() {
    double v = 0.0;
    for (std::uint16_t k = 0; k < _grid._surface.z(); ++k)
        for (std::uint16_t j = 0; j < _grid._surface.y(); ++j)
            for (std::uint16_t i = 0; i < _grid._surface.x(); ++i) {
                const double phi = _grid._surface(i, j, k);
                v += std::clamp(0.5 - phi * 0.5, 0.0, 1.0);
            }
    if (_targetVolume < 0.0) { _targetVolume = v; return v; }
    if (_targetVolume < 1e-6) return v;
    const double rel   = (_targetVolume - v) / _targetVolume;
    const double shift = std::clamp(rel * 0.5, -0.03, 0.03);
    if (std::abs(shift) > 1e-5) {
        for (std::uint16_t k = 0; k < _grid._surface.z(); ++k)
            for (std::uint16_t j = 0; j < _grid._surface.y(); ++j)
                for (std::uint16_t i = 0; i < _grid._surface.x(); ++i) {
                    _grid._surface(i, j, k) = std::clamp(
                            _grid._surface(i, j, k) - shift, -2.0, 2.0);
                }
    }
    return v;
}


const std::vector<std::uint8_t>& Fluids::texture() const {
    return _texture;
}

std::uint16_t Fluids::textureWidth() const {
    return _grid._surface.x();
}

std::uint16_t Fluids::textureHeight() const {
    return _grid._surface.y();
}

std::uint16_t Fluids::textureDepth() const {
    return _grid._surface.z();
}
