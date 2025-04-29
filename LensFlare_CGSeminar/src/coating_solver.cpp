#include "coating_solver.h"

#include <cmath>
#include <pagmo/algorithm.hpp>
#include <pagmo/algorithms/de.hpp>
#include <pagmo/algorithms/cmaes.hpp>
#include <pagmo/algorithms/pso.hpp>
#include <pagmo/algorithms/sade.hpp>
#include <pagmo/archipelago.hpp>
#include <pagmo/population.hpp>
#include <pagmo/island.hpp>

void LensCoatingProblemQW::init(unsigned int num_interfaces, float light_angle_x, float light_angle_y, float lightIntensity) {
    m_num_interfaces = num_interfaces;
    m_light_angle_x = light_angle_x;
    m_light_angle_y = light_angle_y;
    m_light_intensity = lightIntensity;
    m_dim = m_num_interfaces;
    m_lb.resize(m_dim);
    m_ub.resize(m_dim);
    for (int i = 0; i < m_num_interfaces; i++) {
        // lambda:
        m_lb[i] = 380.0;
        m_ub[i] = 740.0;
    }
}

void LensCoatingProblemQW::setRenderObjective(std::vector<glm::vec3>& renderObjective) {
    m_renderObjective = renderObjective;
}

void LensCoatingProblemQW::setLensSystem(LensSystem& lensSystem) {
    m_lensSystem.push_back(lensSystem);
    m_preAptReflectionPairs = lensSystem.getPreAptReflections();
    m_postAptReflectionPairs = lensSystem.getPostAptReflections();

    m_default_Ma = m_lensSystem[0].getMa();
    m_preAptMas = m_lensSystem[0].getMa(m_preAptReflectionPairs);
    m_post_apt_center_ray_x = glm::vec2(-m_light_angle_x / 20 * m_default_Ma[1][0] / m_default_Ma[0][0], m_light_angle_x / 20);
    m_post_apt_center_ray_y = glm::vec2(-m_light_angle_y / 20 * m_default_Ma[1][0] / m_default_Ma[0][0], m_light_angle_y / 20);
    for (auto& const preAptMa : m_preAptMas) {
        m_pre_apt_center_ray_x.push_back(glm::vec2(-m_light_angle_x / 20 * preAptMa[1][0] / preAptMa[0][0], m_light_angle_x / 20));
        m_pre_apt_center_ray_y.push_back(glm::vec2(-m_light_angle_y / 20 * preAptMa[1][0] / preAptMa[0][0], m_light_angle_y / 20));
    }

}

pagmo::vector_double LensCoatingProblemQW::fitness(const pagmo::vector_double& dv) const {
    //Construct lens system
    std::vector<LensInterface> newLensInterfaces;
    newLensInterfaces.reserve(m_num_interfaces);
    std::vector<LensInterface> currentLensInterfaces = m_lensSystem[0].getLensInterfaces();
    for (int i = 0; i < m_num_interfaces; i++) {
        LensInterface lens;
        lens.di = currentLensInterfaces[i].di;
        lens.ni = currentLensInterfaces[i].ni;
        lens.Ri = currentLensInterfaces[i].Ri;
        lens.lambda0 = dv[i];
        newLensInterfaces.push_back(lens);
    }
    LensSystem newLensSystem = LensSystem(m_lensSystem[0].getIrisAperturePos(), m_lensSystem[0].getApertureHeight(), m_lensSystem[0].getEntrancePupilHeight(), newLensInterfaces);

    std::vector<glm::vec3> preAPTtransmissions = newLensSystem.getTransmission(m_preAptReflectionPairs, m_pre_apt_center_ray_x, m_pre_apt_center_ray_y, m_quarterWaveCoating);
    std::vector<glm::vec3> postAPTtransmissions = newLensSystem.getTransmission(m_postAptReflectionPairs, m_post_apt_center_ray_x, m_post_apt_center_ray_y, m_quarterWaveCoating);

    double f = 0.0;

    for (int i = 0; i < preAPTtransmissions.size(); i++) {
        f += pow(glm::length(m_renderObjective[i] - (preAPTtransmissions[i] * m_light_intensity)), 2);
    }

    for (int i = 0; i < postAPTtransmissions.size(); i++) {
        f += pow(glm::length(m_renderObjective[i + preAPTtransmissions.size()] - (postAPTtransmissions[i] * m_light_intensity)), 2);
    }

    f = f / dv.size();

    return { f };
}

std::pair<pagmo::vector_double, pagmo::vector_double> LensCoatingProblemQW::get_bounds() const {
    return { m_lb, m_ub };
}

pagmo::vector_double convertLensSystem(const std::vector<LensInterface>& lens_system) {
    pagmo::vector_double decision;
    decision.reserve(lens_system.size());
    for (const auto& lens : lens_system) {
         decision.push_back(lens.lambda0);
    }
    return decision;
}

std::vector<double> runEACoatings(pagmo::archipelago archi) {
    std::vector<double> c_solution = archi.get_champions_x()[0];
    double c_fitness = archi.get_champions_f()[0][0];
    std::cout << "Initial Best Fitness: " << c_fitness << std::endl;
    std::cout << "Initial Best decision vector: ";
    for (double val : c_solution) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    for (int gen = 0; gen < 15; ++gen) {
        std::cout << "EVOLVING GEN " << gen << std::endl;
        archi.evolve();
        archi.wait();  // Ensure the evolution step is complete

        // Find the best champion across all islands
        double best_fitness = std::numeric_limits<double>::max(); // Initialize to a very large value
        for (const auto& isl : archi) {
            auto island_champion = isl.get_population().champion_f(); // Get the champion fitness
            if (island_champion[0] < best_fitness) { // Check if it's better
                best_fitness = island_champion[0];
            }
        }

        std::cout << "CURRENT BEST FITNESS: " << best_fitness << std::endl;

    }
    //Retrieve and report the best solution.
    double best_fitness = std::numeric_limits<double>::max(); // Initialize to a very large value
    std::vector<double> best_champion;

    for (const auto& isl : archi) {
        auto island_champion = isl.get_population().champion_f(); // Get the champion fitness
        if (island_champion[0] < best_fitness) { // Check if it's better
            best_fitness = island_champion[0];
            best_champion = isl.get_population().champion_x(); // Save the champion decision vector
        }
    }

    // Print the best champion and its fitness
    std::cout << "Best Fitness: " << best_fitness << std::endl;
    std::cout << "Best Champion: ";
    for (const auto& val : best_champion) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    return best_champion;
}

LensSystem solveCoatingAnnotations(LensSystem& currentLensSystem, std::vector<glm::vec3>& renderObjective, float light_angle_x, float light_angle_y, float lightIntensity) {
    
    std::vector<LensInterface> currentLensInterfaces = currentLensSystem.getLensInterfaces();
    unsigned int num_interfaces = currentLensInterfaces.size();
    LensCoatingProblemQW my_problem;
    my_problem.init(num_interfaces, light_angle_x, light_angle_y, lightIntensity);
    my_problem.setRenderObjective(renderObjective);
    my_problem.setLensSystem(currentLensSystem);
    pagmo::problem prob{ my_problem };
    std::cout << "Created Pagmo UDP" << std::endl;

    std::vector<double> current_point = convertLensSystem(currentLensInterfaces);

    //Evolutionary Algorithm
    pagmo::algorithm algo{ pagmo::sade{200} };
    //pagmo::algorithm algo{pagmo::cmaes(100, true, true)};
    //pagmo::algorithm algo{ pagmo::pso(50u, 0.7298, 2.05, 2.05, 0.5, 6u, 2u, 4u, true, pagmo::random_device::next()) };

    pagmo::archipelago archi;
    // Add islands 
    for (int i = 0; i < 25; ++i) {
        pagmo::population pop(prob, 10 * num_interfaces);
        // Add current lens system to the population.
        for (int i = 0; i < 1; i++) {
            pop.push_back(current_point);
        }
        archi.push_back(pagmo::island{ algo, pop });
    }

    std::vector<double> best_champion = runEACoatings(archi);

    //Convert the best decision vector back into a vector of LensInterface
    std::vector<LensInterface> optimized_lens_system;
    for (int i = 0; i < num_interfaces; i++) {
        LensInterface lens;
        lens.di = currentLensInterfaces[i].di;
        lens.ni = currentLensInterfaces[i].ni;
        lens.Ri = currentLensInterfaces[i].Ri;
        lens.lambda0 = best_champion[i];
        optimized_lens_system.push_back(lens);
    }

    // Output the optimized lens interfaces.
    for (size_t i = 0; i < optimized_lens_system.size(); i++) {
        std::cout << "Interface " << i << ": "
            << "di = " << optimized_lens_system[i].di << ", "
            << "ni = " << optimized_lens_system[i].ni << ", "
            << "Ri = " << optimized_lens_system[i].Ri << ", "
            << "lambda0 = " << optimized_lens_system[i].lambda0 << std::endl;
    }

    return LensSystem(currentLensSystem.getIrisAperturePos(), currentLensSystem.getApertureHeight(), currentLensSystem.getEntrancePupilHeight(), optimized_lens_system);

}
