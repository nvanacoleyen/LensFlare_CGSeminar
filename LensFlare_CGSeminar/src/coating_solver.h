#pragma once

#include <iostream>
#include <pagmo/types.hpp>
#include <pagmo/problem.hpp>
#include <vector>
#include "lens_system.h"
#include "quad.h"
#include <glm/glm.hpp>

struct LensCoatingProblemQW {
    unsigned int m_dim;             // total number of decision variables
    pagmo::vector_double m_lb;       // lower bounds for each variable
    pagmo::vector_double m_ub;       // upper bounds for each variable
    unsigned int m_num_interfaces;  // number of lens interfaces
    float m_light_angle_x;
    float m_light_angle_y;
    float m_light_intensity;
    bool m_quarterWaveCoating = true;
    std::vector<glm::vec3> m_renderObjective;
    std::vector<LensSystem> m_lensSystem;
    std::vector<glm::vec2> m_preAptReflectionPairs;
    std::vector<glm::vec2> m_postAptReflectionPairs;

    glm::mat2x2 m_default_Ma;
    std::vector<glm::mat2x2> m_preAptMas;
    glm::vec2 m_post_apt_center_ray_x;
    glm::vec2 m_post_apt_center_ray_y;
    std::vector<glm::vec2> m_pre_apt_center_ray_x;
    std::vector<glm::vec2> m_pre_apt_center_ray_y;


    // Set the problem dimension and bounds
    void init(unsigned int num_interfaces, float light_angle_x, float light_angle_y, float lightIntensity);
    // Set the render objectives for the fitness function
    void setRenderObjective(std::vector<glm::vec3>& renderObjective);
    // Set the current lens system
    void setLensSystem(LensSystem& lensSystem);
    // This function computes the fitness (objective) value.
    pagmo::vector_double fitness(const pagmo::vector_double& dv) const;
    // Get the lower and upper bounds of the decision vector.
    std::pair<pagmo::vector_double, pagmo::vector_double> get_bounds() const;
};

LensSystem solveCoatingAnnotations(LensSystem& currentLensSystem, std::vector<glm::vec3>& renderObjective, float light_angle_x, float light_angle_y, float lightIntensity);
