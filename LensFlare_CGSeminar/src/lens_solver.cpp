#include "lens_solver.h"
#include <vector>
#include <pagmo/algorithm.hpp>
#include <pagmo/algorithms/de.hpp>
#include <pagmo/algorithms/cmaes.hpp>
#include <pagmo/algorithms/pso.hpp>
#include <pagmo/archipelago.hpp>
#include <pagmo/population.hpp>
#include <pagmo/island.hpp>
#include <cmath>
#include <glm/glm.hpp>


int const PARAMS_PER_INTERFACE = 3;

void LensSystemProblem::init(unsigned int num_interfaces, std::vector<LensInterface>& currentLensInterfaces, float light_angle_x, float light_angle_y) {
    m_num_interfaces = num_interfaces;
    m_currentLensInterfaces = currentLensInterfaces;
    m_light_angle_x = light_angle_x;
    m_light_angle_y = light_angle_y;
    m_dim = 1 + (m_num_interfaces * PARAMS_PER_INTERFACE);
    m_lb.resize(m_dim);
    m_ub.resize(m_dim);
    m_lb[0] = 0;
    m_ub[0] = num_interfaces - 1;
    for (int i = 0; i < m_num_interfaces; i++) {
        // di:
        m_lb[1 + (PARAMS_PER_INTERFACE * i)] = 0.1;
        m_ub[1 + (PARAMS_PER_INTERFACE * i)] = 125.0;

        // ni:
        m_lb[1 + (PARAMS_PER_INTERFACE * i) + 1] = 1.0;
        m_ub[1 + (PARAMS_PER_INTERFACE * i) + 1] = 2.5;

        // Ri:
        m_lb[1 + (PARAMS_PER_INTERFACE * i) + 2] = -1000.0;
        m_ub[1 + (PARAMS_PER_INTERFACE * i) + 2] = 1000.0;
    }
}

void LensSystemProblem::setRenderObjective(std::vector<SnapshotData> &renderObjective) {
    sortByQuadHeight(renderObjective);
    m_renderObjective = renderObjective;
}

SnapshotData LensSystemProblem::simulateDrawQuad(int quadId, glm::mat2x2& Ma, glm::mat2x2& Ms, float light_angle_x, float light_angle_y) const {

    glm::vec2 ghost_center_x = glm::vec2(-light_angle_x * Ma[1][0] / Ma[0][0], light_angle_x);
    glm::vec2 ghost_center_y = glm::vec2(light_angle_y * Ma[1][0] / Ma[0][0], -light_angle_y);

    glm::vec2 ghost_center_x_s = Ms * Ma * ghost_center_x;
    glm::vec2 ghost_center_y_s = Ms * Ma * ghost_center_y;
    glm::vec2 ghost_center_pos = glm::vec2(ghost_center_x_s.x, ghost_center_y_s.x);

    // Compute quad height
    glm::vec2 apt_one_x = glm::vec2((1 - (light_angle_x * Ma[1][0])) / Ma[0][0], light_angle_x);
    glm::vec2 apt_one_y = glm::vec2((1 + (light_angle_y * Ma[1][0])) / Ma[0][0], -light_angle_y);
    glm::vec2 apt_one_x_s = Ms * Ma * apt_one_x;
    glm::vec2 apt_one_y_s = Ms * Ma * apt_one_y;
    float ghost_height_factor = glm::length(glm::vec2(apt_one_x_s.x, apt_one_y_s.x) - ghost_center_pos) / glm::length(glm::vec2(1, 1));

    //// Compute initial quad height
    //float initial_quad_height = std::sqrt(
    //    std::pow(light_angle_x, 2.0f) +
    //    std::pow(light_angle_y, 2.0f)
    //);

    //// Compute intensity value
    //float intensityVal = initial_quad_height / quad_height;

    SnapshotData snap;
    snap.quadID = quadId;
    snap.quadCenterPos = ghost_center_pos;
    snap.quadHeight = ghost_height_factor;

    return snap;
}

pagmo::vector_double LensSystemProblem::fitness(const pagmo::vector_double& dv) const {

    //Construct lens system
    std::vector<LensInterface> newLensInterfaces;
    newLensInterfaces.reserve(m_num_interfaces);
    for (int i = 0; i < m_num_interfaces; i++) {
        LensInterface lens;
        lens.di = dv[1 + (PARAMS_PER_INTERFACE * i)];
        lens.ni = dv[1 + (PARAMS_PER_INTERFACE * i) + 1];
        lens.Ri = dv[1 + (PARAMS_PER_INTERFACE * i) + 2];
        lens.hi = m_currentLensInterfaces[i].hi;
        lens.lambda0 = m_currentLensInterfaces[i].lambda0;
        newLensInterfaces.push_back(lens);
    }
    LensSystem newLensSystem = LensSystem(std::round(dv[0]), 0.0, newLensInterfaces);

    //"Render"
    glm::mat2x2 default_Ma = newLensSystem.getMa();
    glm::mat2x2 default_Ms = newLensSystem.getMs();
    std::vector<glm::vec2> preAptReflectionPairs = newLensSystem.getPreAptReflections();
    std::vector<glm::vec2> postAptReflectionPairs = newLensSystem.getPostAptReflections();
    std::vector<glm::mat2x2> preAptMas = newLensSystem.getMa(preAptReflectionPairs);
    std::vector<glm::mat2x2> postAptMss = newLensSystem.getMs(postAptReflectionPairs);

    std::vector<SnapshotData> newSnapshot;
    
    for (int i = 0; i < preAptReflectionPairs.size(); i++) {
        newSnapshot.push_back(simulateDrawQuad(i, preAptMas[i], default_Ms, m_light_angle_x, m_light_angle_y));
    }
    for (int i = 0; i < postAptReflectionPairs.size(); i++) {
        newSnapshot.push_back(simulateDrawQuad(i + preAptReflectionPairs.size(), default_Ma, postAptMss[i], m_light_angle_x, m_light_angle_y));
    }

    sortByQuadHeight(newSnapshot);

    //Compute fitness
    double f = 0.0;

    int forloopsize = m_renderObjective.size();
    if (forloopsize > newSnapshot.size()) {
        forloopsize = newSnapshot.size();
    }

    for (int i = 0; i < forloopsize; i++) {
        //Change to sort all ghosts on size and compare that way instead of on id
        float posError = glm::length(m_renderObjective[i].quadCenterPos - newSnapshot[i].quadCenterPos);
        float sizeError = 40 * abs(m_renderObjective[i].quadHeight - newSnapshot[i].quadHeight);
        f += sizeError + posError;
    }    

    return { f };
}

std::pair<pagmo::vector_double, pagmo::vector_double> LensSystemProblem::get_bounds() const {
    return { m_lb, m_ub };
}

//Convert a vector of LensInterface into a decision vector.
pagmo::vector_double convertLensSystem(unsigned int aptPos, const std::vector<LensInterface>& lens_system) {
    pagmo::vector_double decision;
    decision.reserve(1 + (lens_system.size() * PARAMS_PER_INTERFACE));
    decision.push_back(aptPos);
    for (const auto& lens : lens_system) {
        decision.push_back(lens.di);
        decision.push_back(lens.ni);
        decision.push_back(lens.Ri);
        // to optimize lambda0 as well, uncomment the next line and update dimension and bounds.
        // decision.push_back(lens.lambda0);
    }
    return decision;
}

void sortByQuadHeight(std::vector<SnapshotData>& snapshotDataUnsorted) {
    std::sort(snapshotDataUnsorted.begin(), snapshotDataUnsorted.end(),
        [](const SnapshotData& a, const SnapshotData& b) {
            return a.quadHeight < b.quadHeight;
        });
}

LensSystem solve_Annotations(LensSystem& currentLensSystem, std::vector<SnapshotData> renderObjective, float light_angle_x, float light_angle_y) {

    std::vector<LensInterface> currentLensInterfaces = currentLensSystem.getLensInterfaces();
    unsigned int num_interfaces = currentLensInterfaces.size();
    LensSystemProblem my_problem;
    my_problem.init(num_interfaces, currentLensInterfaces, light_angle_x, light_angle_y);
    my_problem.setRenderObjective(renderObjective);
    pagmo::problem prob{ my_problem };
    std::cout << "Created Pagmo UDP" << std::endl;

    // Convert the current lens system to a decision vector.
    std::vector<double> current_point = convertLensSystem(currentLensSystem.getIrisAperturePos(), currentLensInterfaces);
    //Differential Evolution (DE) with 100 generations per evolve call.
    pagmo::algorithm algo{ pagmo::de{100} };
    //pagmo::algorithm algo{pagmo::cmaes(100, true, true)};
    //pagmo::algorithm algo{ pagmo::pso(50u, 0.7298, 2.05, 2.05, 0.5, 6u, 2u, 4u, true, pagmo::random_device::next()) };
    // Create a population.
    unsigned int amount_dv = current_point.size();

    pagmo::archipelago archi;
    // Add islands 
    for (int i = 0; i < 25; ++i) {
        pagmo::population pop(prob, 10 * amount_dv);
        // Add current lens system to the population.
        //for (int i = 0; i < 1; i++) {
        //    pop.push_back(current_point);
        //}
        archi.push_back(pagmo::island{ algo, pop });
    }


    std::vector<double> c_solution = archi.get_champions_x()[0];
    double c_fitness = archi.get_champions_f()[0][0];
    std::cout << "Initial Best Fitness: " << c_fitness << std::endl;
    std::cout << "Initial Best decision vector: ";
    for (double val : c_solution) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    std::cout << "Created Population" << std::endl;

    
    //Option 1: Evolve the population directly.
     //for (int i = 0; i < 100; ++i) {
     //    pop = algo.evolve(pop);
     //    std::cout << "evolve gen " << i << std::endl;
     //    std::cout << "CURRENT BEST FITNESS: " << pop.champion_f()[0] << std::endl;
     //}
     //pagmo::population final_pop = pop;

    //Option 2: use an island (useful for parallel/distributed runs).
    /*pagmo::island isl(algo, pop);*/
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

    //Convert the best decision vector back into a vector of LensInterface
    std::vector<LensInterface> optimized_lens_system;
    for (int i = 0; i < num_interfaces; i++) {
        LensInterface lens;
        lens.di = best_champion[1 + (PARAMS_PER_INTERFACE * i)];
        lens.ni = best_champion[1 + (PARAMS_PER_INTERFACE * i) + 1];
        lens.Ri = best_champion[1 + (PARAMS_PER_INTERFACE * i) + 2];
        lens.hi = currentLensInterfaces[i].hi;
        lens.lambda0 = currentLensInterfaces[i].lambda0;
        optimized_lens_system.push_back(lens);
    }

    // Output the optimized lens interfaces.
    for (size_t i = 0; i < optimized_lens_system.size(); i++) {
        std::cout << "Interface " << i << ": "
            << "di = " << optimized_lens_system[i].di << ", "
            << "ni = " << optimized_lens_system[i].ni << ", "
            << "Ri = " << optimized_lens_system[i].Ri << ", "
            << "hi = " << optimized_lens_system[i].hi << ", "
            << "lambda0 = " << optimized_lens_system[i].lambda0 << std::endl;
    }

    return LensSystem(std::round(best_champion[0]), 0.0, optimized_lens_system);
}
