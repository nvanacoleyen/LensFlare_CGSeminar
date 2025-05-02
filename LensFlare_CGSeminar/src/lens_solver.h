#pragma once

#include <iostream>
#include <pagmo/types.hpp>
#include <pagmo/problem.hpp>
#include "lens_system.h"
#include "quad.h"

struct LensSystemProblem {
    unsigned int m_num_interfaces;  // number of lens interfaces
    float m_light_angle_x;
    float m_light_angle_y;
    unsigned int m_dim;             // total number of decision variables
    float m_entrance_pupil_height = 50.f;
    pagmo::vector_double m_lb;       // lower bounds for each variable
    pagmo::vector_double m_ub;       // upper bounds for each variable
    std::vector<SnapshotData> m_renderObjective;
    //Simulate drawing a quad, only necessary info for snapshot
    SnapshotData simulateDrawQuad(int quadId, glm::mat2x2& Ma, glm::mat2x2& Ms, float light_angle_x, float light_angle_y, float irisApertureHeight) const;
    // Set the problem dimension and bounds
    void init(unsigned int num_interfaces, float light_angle_x, float light_angle_y);
    // Set the render objectives for the fitness function
    void setRenderObjective(std::vector<SnapshotData> &renderObjective);
    // This function computes the fitness (objective) value.
    pagmo::vector_double fitness(const pagmo::vector_double& dv) const;
    // Get the lower and upper bounds of the decision vector.
    std::pair<pagmo::vector_double, pagmo::vector_double> get_bounds() const;
};

void sortByQuadHeight(std::vector<SnapshotData>& snapshotDataUnsorted);
std::vector<LensSystem> solveLensAnnotations(LensSystem& currentLensSystem, std::vector<SnapshotData>& renderObjective, float light_angle_x, float light_angle_y);
std::vector<LensSystem> solveLensAnnotations(std::vector<SnapshotData>& renderObjective, float light_angle_x, float light_angle_y);
