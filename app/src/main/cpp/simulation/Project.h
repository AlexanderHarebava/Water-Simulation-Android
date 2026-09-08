
#pragma once

#include <algorithm>

#include "./types.h"
#include "./StaggeredGrid.h"
#include "./ConjugateGradient.h"

class Project
{
public:
    explicit Project(
            StaggeredGrid<double, std::uint16_t>& grid
    ) : _grid(grid) {}
    virtual ~Project() = default;
    virtual void project() = 0;
protected:
    Eigen::SparseMatrix<double> _A;

    Eigen::VectorXd _x, _b, _r, _z, _s;
    std::vector<Eigen::Triplet<double>> _triplets;
    StaggeredGrid<double, std::uint16_t>& _grid;
};

class Project2D : public Project
{
using Project::Project;
 public:
    void project() override;

 private:
    void preparePressureSolving(
            Eigen::SparseMatrix<double>& A,
            Eigen::VectorXd& b
        );
    inline double div(
            const std::uint16_t i,
            const std::uint16_t j,
            const std::uint16_t k
        ) const;
};


class Project3D : public Project
{
using Project::Project;
 public:
    void project() override;

 private:
    void preparePressureSolving(
            Eigen::SparseMatrix<double>& A,
            Eigen::VectorXd& b
        );
    inline double div(
            const std::uint16_t i,
            const std::uint16_t j,
            const std::uint16_t k
        ) const;
};
