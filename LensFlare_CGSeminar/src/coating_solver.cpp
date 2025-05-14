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
#include "utils.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
#include <sstream>

void LensCoatingProblem::init(unsigned int num_interfaces, float light_angle_x, float light_angle_y, float lightIntensity, bool quarterWaveCoating) {
    m_num_interfaces = num_interfaces;
    m_light_angle_x = light_angle_x;
    m_light_angle_y = light_angle_y;
    m_light_intensity = lightIntensity;
    m_quarterWaveCoating = quarterWaveCoating;
    if (quarterWaveCoating) {
		m_dim = m_num_interfaces;
	}
    else {
        m_dim = m_num_interfaces * 2;
    }
    m_lb.resize(m_dim);
    m_ub.resize(m_dim);
    for (int i = 0; i < m_num_interfaces; i++) {
        if (quarterWaveCoating) {
            // lambda:
            m_lb[i] = 380.0;
            m_ub[i] = 740.0;
        }
        else {
            // c_di:
            m_lb[i * 2] = 25.0;
            m_ub[i * 2] = 750.0;
            // c_ni:
            m_lb[i * 2 + 1] = 1.38;
            m_ub[i * 2 + 1] = 1.9;
        }
    }
}

void LensCoatingProblem::setRenderObjective(std::vector<glm::vec3>& renderObjective) {
    m_renderObjective = renderObjective;
}

void LensCoatingProblem::setLensSystem(LensSystem& lensSystem) {
    m_lensSystem.push_back(lensSystem);
    m_preAptReflectionPairs = lensSystem.getPreAptReflections();
    m_postAptReflectionPairs = lensSystem.getPostAptReflections();

    m_default_Ma = m_lensSystem[0].getMa();
    m_preAptMas = m_lensSystem[0].getMa(m_preAptReflectionPairs);
    m_post_apt_center_ray_x = glm::vec2(-m_light_angle_x * m_default_Ma[1][0] / m_default_Ma[0][0], m_light_angle_x);
    m_post_apt_center_ray_y = glm::vec2(-m_light_angle_y * m_default_Ma[1][0] / m_default_Ma[0][0], m_light_angle_y);
    for (auto& const preAptMa : m_preAptMas) {
        m_pre_apt_center_ray_x.push_back(glm::vec2(-m_light_angle_x * preAptMa[1][0] / preAptMa[0][0], m_light_angle_x));
        m_pre_apt_center_ray_y.push_back(glm::vec2(-m_light_angle_y * preAptMa[1][0] / preAptMa[0][0], m_light_angle_y));
    }

}

pagmo::vector_double LensCoatingProblem::fitness(const pagmo::vector_double& dv) const {
    //Construct lens system
    std::vector<LensInterface> newLensInterfaces;
    newLensInterfaces.reserve(m_num_interfaces);
    std::vector<LensInterface> currentLensInterfaces = m_lensSystem[0].getLensInterfaces();
    for (int i = 0; i < m_num_interfaces; i++) {
        LensInterface lens;
        lens.di = currentLensInterfaces[i].di;
        lens.ni = currentLensInterfaces[i].ni;
        lens.Ri = currentLensInterfaces[i].Ri;
		if (m_quarterWaveCoating) {
			lens.lambda0 = dv[i];
        }
        else {
			lens.c_di = dv[i * 2];
			lens.c_ni = dv[i * 2 + 1];
        }
        newLensInterfaces.push_back(lens);
    }
    LensSystem newLensSystem = LensSystem(m_lensSystem[0].getIrisAperturePos(), m_lensSystem[0].getApertureHeight(), m_lensSystem[0].getEntrancePupilHeight(), newLensInterfaces);

    std::vector<glm::vec3> preAPTtransmissions = newLensSystem.getTransmission(m_preAptReflectionPairs, m_pre_apt_center_ray_x, m_pre_apt_center_ray_y, m_quarterWaveCoating);
    std::vector<glm::vec3> postAPTtransmissions = newLensSystem.getTransmission(m_postAptReflectionPairs, m_post_apt_center_ray_x, m_post_apt_center_ray_y, m_quarterWaveCoating);

    double f = 0.0;

    for (size_t i = 0; i < preAPTtransmissions.size(); i++) {
        glm::vec3 normalizedObjective = normalizeRGB(m_renderObjective[i]);
        glm::vec3 normalizedTransmitted = normalizeRGB(preAPTtransmissions[i] * m_light_intensity);

        f += glm::length(normalizedObjective - normalizedTransmitted);
    }

    for (size_t i = 0; i < postAPTtransmissions.size(); i++) {
        glm::vec3 normalizedObjective = normalizeRGB(m_renderObjective[i + preAPTtransmissions.size()]);
        glm::vec3 normalizedTransmitted = normalizeRGB(postAPTtransmissions[i] * m_light_intensity);

        f += glm::length(normalizedObjective - normalizedTransmitted);
    }

    f = f / dv.size();
    return { f };
}

std::pair<pagmo::vector_double, pagmo::vector_double> LensCoatingProblem::get_bounds() const {
    return { m_lb, m_ub };
}

pagmo::vector_double convertLensSystemQW(const std::vector<LensInterface>& lens_system) {
    pagmo::vector_double decision;
    decision.reserve(lens_system.size());
    for (const auto& lens : lens_system) {
         decision.push_back(lens.lambda0);
    }
    return decision;
}

pagmo::vector_double convertLensSystemCustom(const std::vector<LensInterface>& lens_system) {
    pagmo::vector_double decision;
    decision.reserve(lens_system.size());
    for (const auto& lens : lens_system) {
        decision.push_back(lens.c_di);
        decision.push_back(lens.c_ni);
    }
    return decision;
}

std::vector<double> runEACoatings(pagmo::archipelago archi) {
    std::ofstream csvFile("ea_log.csv", std::ios::app);
    if (!csvFile.is_open()) {
        std::cerr << "Error opening CSV log file!" << std::endl;
    }
    csvFile << "######################################################################" << std::endl;
    csvFile << "EA Coatings Run" << std::endl;
    csvFile << "Generation,Elapsed Time (sec),Total Evaluations,Best Fitness" << std::endl;

    std::vector<double> c_solution = archi.get_champions_x()[0];
    double c_fitness = archi.get_champions_f()[0][0];
    std::cout << "Initial Best Fitness: " << c_fitness << std::endl;
    std::cout << "Initial Best decision vector: ";
    for (double val : c_solution) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    unsigned long long total_fevals = 0;

    for (int gen = 0; gen < 10; ++gen) {
        std::cout << "EVOLVING GEN " << gen << std::endl;
        archi.evolve();
        archi.wait();  // Ensure the evolution step is complete

        double best_fitness = std::numeric_limits<double>::max();
        for (const auto& isl : archi) {
            auto island_champion = isl.get_population().champion_f();
            if (island_champion[0] < best_fitness) {
                best_fitness = island_champion[0];
            }
        }
        std::cout << "CURRENT BEST FITNESS: " << best_fitness << std::endl;

        for (const auto& isl : archi) {
            total_fevals += isl.get_population().get_problem().get_fevals();
        }
        std::cout << "Total function evaluations after gen " << gen << ": " << total_fevals << std::endl;

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed_secs = std::chrono::duration_cast<std::chrono::seconds>(currentTime - start).count();

        csvFile << gen << ","
            << elapsed_secs << ","
            << total_fevals << ","
            << best_fitness << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_secs = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    int minutes = static_cast<int>(elapsed_secs / 60);
    int seconds = static_cast<int>(elapsed_secs % 60);
    std::cout << "Computation time: " << minutes << " minutes and " << seconds << " seconds" << std::endl;
    csvFile << std::endl;
    csvFile << "Final Computation Time (min:sec):," << minutes << ":" << seconds << std::endl;
    csvFile << "Total Function Evaluations:," << total_fevals << std::endl;

    double best_fitness = std::numeric_limits<double>::max();
    std::vector<double> best_champion;
    for (const auto& isl : archi) {
        auto island_champion = isl.get_population().champion_f();
        if (island_champion[0] < best_fitness) {
            best_fitness = island_champion[0];
            best_champion = isl.get_population().champion_x();
        }
    }

    std::cout << "Best Fitness: " << best_fitness << std::endl;
    std::cout << "Best Champion: ";
    for (const auto& val : best_champion) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    csvFile.close();
    return best_champion;
}


LensSystem solveCoatingAnnotations(LensSystem& currentLensSystem, std::vector<glm::vec3>& renderObjective, float light_angle_x, float light_angle_y, float lightIntensity, bool quarterWaveCoating) {
    
    std::vector<LensInterface> currentLensInterfaces = currentLensSystem.getLensInterfaces();
    unsigned int num_interfaces = currentLensInterfaces.size();
    
    LensCoatingProblem my_problem;
    my_problem.init(num_interfaces, 0.001f, 0.001f, lightIntensity, quarterWaveCoating);
    my_problem.setRenderObjective(renderObjective);
    my_problem.setLensSystem(currentLensSystem);
    pagmo::problem prob{ my_problem };
    
    std::cout << "Created Pagmo UDP" << std::endl;

    std::vector<double> current_point;
    if (quarterWaveCoating) {
        current_point = convertLensSystemQW(currentLensInterfaces);
    }
    else {
		current_point = convertLensSystemCustom(currentLensInterfaces);
    }
     

    //Evolutionary Algorithm
    pagmo::algorithm algo{ pagmo::pso{200} };
    //pagmo::algorithm algo{pagmo::cmaes(100, true, true)};
    //pagmo::algorithm algo{ pagmo::pso(50u, 0.7298, 2.05, 2.05, 0.5, 6u, 2u, 4u, true, pagmo::random_device::next()) };

    pagmo::archipelago archi;
    // Add islands 
    for (int i = 0; i < 15; ++i) {
        pagmo::population pop(prob, 15 * num_interfaces);
        // Add current lens system to the population.
        //for (int i = 0; i < 1; i++) {
        //    pop.push_back(current_point);
        //}
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
		if (quarterWaveCoating) {
            lens.lambda0 = best_champion[i];
        }
        else {
			lens.c_di = best_champion[i * 2];
			lens.c_ni = best_champion[i * 2 + 1];
        }
        
        optimized_lens_system.push_back(lens);
    }

    // Output the optimized lens interfaces.
    //for (size_t i = 0; i < optimized_lens_system.size(); i++) {
    //    std::cout << "Interface " << i << ": "
    //        << "di = " << optimized_lens_system[i].di << ", "
    //        << "ni = " << optimized_lens_system[i].ni << ", "
    //        << "Ri = " << optimized_lens_system[i].Ri << ", "
    //        << "lambda0 = " << optimized_lens_system[i].lambda0 << std::endl;
    //}

    return LensSystem(currentLensSystem.getIrisAperturePos(), currentLensSystem.getApertureHeight(), currentLensSystem.getEntrancePupilHeight(), optimized_lens_system);

}
