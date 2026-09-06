
#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <android/log.h>
#include <algorithm>
#include <cmath>
#include "types.h"
#include "config.h"
#include "StaggeredGrid.h"
#include "Advect.h"
#include "Project.h"

class Fluids {
public:
    explicit Fluids();

    void update(const std::uint64_t iteration);
    void setGravity(float gx, float gy, float gz);

    void setDamping(float damping) {
        _damping = static_cast<double>(damping);
    }
    void setWaterAmount(float amount) {
        amount = std::clamp(amount, 0.05f, 2.0f);

        if (std::abs(amount - _waterAmount) > 1e-5f) {
            _waterAmount = amount;
            _waterAmountDirty = true;
        }
    }
    const std::vector<std::uint8_t>& texture() const;

    std::uint16_t textureWidth() const;
    std::uint16_t textureHeight() const;
    std::uint16_t textureDepth() const;

private:
    void step();
    void initSphere();

    float _waterAmount = 1.0f;
    bool _waterAmountDirty = false;
    void addForces();
    void initSurface();
    double conserveVolume();
    double _targetVolume = -1.0;

    void redistancing(
            const std::uint64_t nbIte,
            Field<double, std::uint16_t>& field,
            Field<double, std::uint16_t>& fieldTemp
    );

    void extrapolate(
            Field<double, std::uint16_t>& F,
            Field<double, std::uint16_t>& Ftemp,
            std::uint16_t nbIte = 0
    ) const;
    double _damping = 0.998;
    void updateTexture3D();

    std::uint64_t _iteration = 0;

    std::vector<std::uint8_t> _texture;
    Field<double, std::uint16_t> _rdF{Config::N, Config::N, Config::N};
    Field<double, std::uint16_t> _rdQ{Config::N, Config::N, Config::N};
    Field<double, std::uint16_t> _rdN{Config::N, Config::N, Config::N};
    StaggeredGrid<double, std::uint16_t> _grid {Config::N};

    std::unique_ptr<Advect> _advection;
    std::unique_ptr<Project> _projection;

    float _gravityX = 0.0f;
    float _gravityY = -2.0f;
    float _gravityZ = 0.0f;
};
