#include "lens_solver.h"
#include <vector>
#include <pagmo/algorithm.hpp>
#include <pagmo/algorithms/de.hpp>
#include <pagmo/algorithms/cmaes.hpp>
#include <pagmo/algorithms/pso.hpp>
#include <pagmo/algorithms/sade.hpp>
#include <pagmo/archipelago.hpp>
#include <pagmo/population.hpp>
#include <pagmo/island.hpp>
#include <cmath>
#include <glm/glm.hpp>


int const PARAMS_PER_INTERFACE = 3;

void LensSystemProblem::init(unsigned int num_interfaces, float light_angle_x, float light_angle_y) {
    m_num_interfaces = num_interfaces;
    m_light_angle_x = light_angle_x;
    m_light_angle_y = light_angle_y;
    m_dim = 3 + (m_num_interfaces * PARAMS_PER_INTERFACE);
    m_lb.resize(m_dim);
    m_ub.resize(m_dim);
    //apt pos
    m_lb[0] = 0;
    m_ub[0] = num_interfaces - 1;
	//apt height
    m_lb[1] = 1;
    m_ub[1] = 20;
	//entrance pupil height
    m_lb[2] = 10;
    m_ub[2] = 20;
    for (int i = 1; i < m_num_interfaces + 1; i++) {
        // di:
        m_lb[(PARAMS_PER_INTERFACE * i)] = 0.1;
        m_ub[(PARAMS_PER_INTERFACE * i)] = 125.0;

        // ni:
        m_lb[(PARAMS_PER_INTERFACE * i) + 1] = 1.0;
        m_ub[(PARAMS_PER_INTERFACE * i) + 1] = 2.5;

        // Ri:
        m_lb[(PARAMS_PER_INTERFACE * i) + 2] = -1000.0;
        m_ub[(PARAMS_PER_INTERFACE * i) + 2] = 1000.0;
    }
}

void LensSystemProblem::setRenderObjective(std::vector<SnapshotData> &renderObjective) {
    sortByQuadHeight(renderObjective);
    m_renderObjective = renderObjective;
}

SnapshotData LensSystemProblem::simulateDrawQuad(int quadId, glm::mat2x2& Ma, glm::mat2x2& Ms, float light_angle_x, float light_angle_y, float irisApertureHeight, float entrancePupilHeight) const {

    //Projection of the aperture center onto the sensor
    glm::vec2 ghost_center_x = glm::vec2(-light_angle_x * Ma[1][0] / Ma[0][0], light_angle_x);
    glm::vec2 ghost_center_y = glm::vec2(-light_angle_y * Ma[1][0] / Ma[0][0], light_angle_y);

    glm::vec2 ghost_center_x_s = Ms * Ma * ghost_center_x;
    glm::vec2 ghost_center_y_s = Ms * Ma * ghost_center_y;
    glm::vec2 ghost_center_pos = glm::vec2(ghost_center_x_s.x, ghost_center_y_s.x);

    glm::vec2 apt_h_x = glm::vec2(((irisApertureHeight / 2.0) - (light_angle_x * Ma[1][0])) / Ma[0][0], light_angle_x);
    glm::vec2 apt_h_x_s = Ms * Ma * apt_h_x;
    float ghost_height = abs(apt_h_x_s.x - ghost_center_pos.x);

	glm::vec2 entrance_pupil_h_x_s = Ms * Ma * glm::vec2((entrancePupilHeight / 2.0), light_angle_x);
    glm::vec2 entrance_pupil_center_x_s = Ms * Ma * glm::vec2(0.f, light_angle_x);
    glm::vec2 entrance_pupil_center_y_s = Ms * Ma * glm::vec2(0.f, light_angle_y);
	float entrance_pupil_height = abs(entrance_pupil_h_x_s.x - entrance_pupil_center_x_s.x);
	glm::vec2 entrance_pupil_center_pos = glm::vec2(entrance_pupil_center_x_s.x, entrance_pupil_center_y_s.x);

    //// Compute intensity value
    //float intensityVal = initial_quad_height / quad_height;

    // Currently has no clue if the ghost is clipped by the entrance pupil or not!
    bool ghost_center_clipped = false;
    float dist_between_centers = glm::length(entrance_pupil_center_pos - ghost_center_pos);
    if (entrancePupilHeight < dist_between_centers && ghost_height < dist_between_centers) {
        ghost_center_clipped = true;
    }

    SnapshotData snap;
    snap.quadID = quadId;
    if (ghost_center_clipped) {
        snap.quadCenterPos = ghost_center_pos;
		snap.quadHeight = 100000; //since we sort on quad height, this will be the last one
    }
    else if (entrance_pupil_height < ghost_height) {
        snap.quadCenterPos = entrance_pupil_center_pos;
        snap.quadHeight = entrance_pupil_height;
    }
    else {
        snap.quadCenterPos = ghost_center_pos;
        snap.quadHeight = ghost_height;
    }

    return snap;
}

pagmo::vector_double LensSystemProblem::fitness(const pagmo::vector_double& dv) const {

    //Construct lens system
    std::vector<LensInterface> newLensInterfaces;
    newLensInterfaces.reserve(m_num_interfaces);
    for (int i = 1; i < m_num_interfaces + 1; i++) {
        LensInterface lens;
        lens.di = dv[(PARAMS_PER_INTERFACE * i)];
        lens.ni = dv[(PARAMS_PER_INTERFACE * i) + 1];
        lens.Ri = dv[(PARAMS_PER_INTERFACE * i) + 2];
        newLensInterfaces.push_back(lens);
    }
    LensSystem newLensSystem = LensSystem(std::round(dv[0]), dv[1], dv[2], newLensInterfaces);

    //"Render"
    glm::mat2x2 default_Ma = newLensSystem.getMa();
    glm::mat2x2 default_Ms = newLensSystem.getMs();
    std::vector<glm::vec2> preAptReflectionPairs = newLensSystem.getPreAptReflections();
    std::vector<glm::vec2> postAptReflectionPairs = newLensSystem.getPostAptReflections();

	if (preAptReflectionPairs.size() + postAptReflectionPairs.size() < m_renderObjective.size()) {
		return { 100000.0 };
	}

    std::vector<glm::mat2x2> preAptMas = newLensSystem.getMa(preAptReflectionPairs);
    std::vector<glm::mat2x2> postAptMss = newLensSystem.getMs(postAptReflectionPairs);

    std::vector<SnapshotData> newSnapshot;
    
    for (int i = 0; i < preAptReflectionPairs.size(); i++) {
        newSnapshot.push_back(simulateDrawQuad(i, preAptMas[i], default_Ms, m_light_angle_x, m_light_angle_y, dv[1], dv[2]));
    }
    for (int i = 0; i < postAptReflectionPairs.size(); i++) {
        newSnapshot.push_back(simulateDrawQuad(i + preAptReflectionPairs.size(), default_Ma, postAptMss[i], m_light_angle_x, m_light_angle_y, dv[1], dv[2]));
    }

    // Compare ghosts on size (directly related to intensity)
    sortByQuadHeight(newSnapshot);

    //TODO : add weight for extra ghosts that have high intensity
    //Compute fitness
    double f = 0.0;

    for (int i = 0; i < m_renderObjective.size(); i++) {
        float posError = glm::length(m_renderObjective[i].quadCenterPos - newSnapshot[i].quadCenterPos);
        float sizeError = m_renderObjective[i].quadHeight - newSnapshot[i].quadHeight;
		f += 3 * (sizeError * sizeError) + (posError * posError); //square because human perception more sensitive to big errors than small ones
    } 

    if (newSnapshot.size() > m_renderObjective.size()) {
        for (int i = m_renderObjective.size(); i < newSnapshot.size(); i++) {
			f += 10 / newSnapshot[i].quadHeight; // penalize extra ghosts proportional to their size
        }
    }

    f = f / m_renderObjective.size();

    return { f };
}

std::pair<pagmo::vector_double, pagmo::vector_double> LensSystemProblem::get_bounds() const {
    return { m_lb, m_ub };
}

//Convert a vector of LensInterface into a decision vector.
pagmo::vector_double convertLensSystem(unsigned int aptPos, const std::vector<LensInterface>& lens_system, float irisApertureHeight, float entrancePupilHeight) {
    pagmo::vector_double decision;
    decision.reserve(1 + (lens_system.size() * PARAMS_PER_INTERFACE));
    decision.push_back(aptPos);
    decision.push_back(irisApertureHeight);
    decision.push_back(entrancePupilHeight);
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

std::vector<double> runEA(pagmo::archipelago archi) {
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

LensSystem solveLensAnnotations(LensSystem& currentLensSystem, std::vector<SnapshotData>& renderObjective, float light_angle_x, float light_angle_y) {

    std::vector<LensInterface> currentLensInterfaces = currentLensSystem.getLensInterfaces();
    unsigned int num_interfaces = currentLensInterfaces.size();
    LensSystemProblem my_problem;
    my_problem.init(num_interfaces, light_angle_x, light_angle_y);
    my_problem.setRenderObjective(renderObjective);
    pagmo::problem prob{ my_problem };
    std::cout << "Created Pagmo UDP" << std::endl;

    // Convert the current lens system to a decision vector.
    std::vector<double> current_point = convertLensSystem(currentLensSystem.getIrisAperturePos(), currentLensInterfaces, currentLensSystem.getApertureHeight(), currentLensSystem.getEntrancePupilHeight());

    //Evolutionary Algorithm
    pagmo::algorithm algo{ pagmo::sade{200} };
    //pagmo::algorithm algo{pagmo::cmaes(100, true, true)};
    //pagmo::algorithm algo{ pagmo::pso(50u, 0.7298, 2.05, 2.05, 0.5, 6u, 2u, 4u, true, pagmo::random_device::next()) };
    // 
    // Create a population.
    unsigned int amount_dv = current_point.size();
    pagmo::archipelago archi;
    // Add islands 
    for (int i = 0; i < 25; ++i) {
        pagmo::population pop(prob, 10 * amount_dv);
        // Add current lens system to the population.
        /*for (int i = 0; i < 1; i++) {
            pop.push_back(current_point);
        }*/
        archi.push_back(pagmo::island{ algo, pop });
    }

	std::vector<double> best_champion = runEA(archi);

    //Convert the best decision vector back into a vector of LensInterface
    std::vector<LensInterface> optimized_lens_system;
    for (int i = 1; i < num_interfaces + 1; i++) {
        LensInterface lens;
        lens.di = best_champion[(PARAMS_PER_INTERFACE * i)];
        lens.ni = best_champion[(PARAMS_PER_INTERFACE * i) + 1];
        lens.Ri = best_champion[(PARAMS_PER_INTERFACE * i) + 2];
        lens.lambda0 = currentLensInterfaces[i-1].lambda0;
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

    return LensSystem(std::round(best_champion[0]), best_champion[1], best_champion[2], optimized_lens_system);
}

int combination2(int n) {
    return (n >= 2) ? (n * (n - 1)) / 2 : 0;
}

int ghostCountForInterfaces(int n) {
    int totalNonAperture = n - 1;
    int left = totalNonAperture / 2;         // Floor division.
    int right = totalNonAperture - left;       // The remaining interfaces.
    return combination2(left) + combination2(right);
}

int interfacesNeeded(int ghostsWanted) {
    // We start at n = 4, the minimum for any non-zero ghost count.
    int n = 4;
    while (true) {
        int ghostsProduced = ghostCountForInterfaces(n);
        if (ghostsProduced >= ghostsWanted)
            break;
        n++;
    }
    std::cout << "Interfaces needed: " << n << ", for " << ghostsWanted << " Ghosts" << std::endl;
	std::cout << "Ghosts produced: " << ghostCountForInterfaces(n) << std::endl;
    return n;
}

LensSystem solveLensAnnotations(std::vector<SnapshotData>& renderObjective, float light_angle_x, float light_angle_y) {

    unsigned int num_interfaces = interfacesNeeded(renderObjective.size());

    LensSystemProblem my_problem;
    my_problem.init(num_interfaces, light_angle_x, light_angle_y);
    my_problem.setRenderObjective(renderObjective);
    pagmo::problem prob{ my_problem };
    std::cout << "Created Pagmo UDP" << std::endl;

    // Convert the current lens system to a decision vector.
    //Differential Evolution (DE) with 100 generations per evolve call.
    pagmo::algorithm algo{ pagmo::sade{200} };
    //pagmo::algorithm algo{pagmo::cmaes(100, true, true)};
    //pagmo::algorithm algo{ pagmo::pso(50u, 0.7298, 2.05, 2.05, 0.5, 6u, 2u, 4u, true, pagmo::random_device::next()) };
    // 
    // Create a population.
    unsigned int amount_dv = 3 + (num_interfaces * PARAMS_PER_INTERFACE);
    pagmo::archipelago archi;
    // Add islands 
    for (int i = 0; i < 25; ++i) {
        pagmo::population pop(prob, 10 * amount_dv);
        archi.push_back(pagmo::island{ algo, pop });
    }

    std::vector<double> best_champion = runEA(archi);

    //Convert the best decision vector back into a vector of LensInterface
    std::vector<LensInterface> optimized_lens_system;
    for (int i = 1; i < num_interfaces + 1; i++) {
        LensInterface lens;
        lens.di = best_champion[(PARAMS_PER_INTERFACE * i)];
        lens.ni = best_champion[(PARAMS_PER_INTERFACE * i) + 1];
        lens.Ri = best_champion[(PARAMS_PER_INTERFACE * i) + 2];
        lens.lambda0 = 440;
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

    return LensSystem(std::round(best_champion[0]), best_champion[1], best_champion[2], optimized_lens_system);
}
