#include "lens_solver.h"
#include <vector>
#include <pagmo/algorithm.hpp>
#include <pagmo/algorithms/de.hpp>
#include <pagmo/algorithms/cmaes.hpp>
#include <pagmo/algorithms/pso.hpp>
#include <pagmo/algorithms/sade.hpp>
#include <pagmo/algorithms/pso_gen.hpp>
#include <pagmo/algorithms/de1220.hpp>
#include <pagmo/algorithms/gaco.hpp>
#include <pagmo/algorithms/bee_colony.hpp>
#include <pagmo/archipelago.hpp>
#include <pagmo/population.hpp>
#include <pagmo/island.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <chrono>
#include <fstream>
#include <sstream>
#include <pagmo/bfe.hpp>

int const PARAMS_PER_INTERFACE = 3;

void LensSystemProblem::init(unsigned int num_interfaces, float light_angle_x, float light_angle_y) {
    m_num_interfaces = num_interfaces;
    m_light_angle_x = light_angle_x;
    m_light_angle_y = light_angle_y;
    m_dim = 2 + (m_num_interfaces * PARAMS_PER_INTERFACE);
    m_lb.resize(m_dim);
    m_ub.resize(m_dim);
    //apt pos
    m_lb[0] = 0;
    m_ub[0] = num_interfaces - 1;
	//apt height
    m_lb[1] = 1;
    m_ub[1] = 50;
    for (int i = 0; i < m_num_interfaces; i++) {
        // di:
        m_lb[2 + (PARAMS_PER_INTERFACE * i)] = 0.1;
        m_ub[2 + (PARAMS_PER_INTERFACE * i)] = 100.0;

        // ni:
        m_lb[2 + (PARAMS_PER_INTERFACE * i) + 1] = 1.0;
        m_ub[2 + (PARAMS_PER_INTERFACE * i) + 1] = 1.75;

        // Ri:
        m_lb[2 + (PARAMS_PER_INTERFACE * i) + 2] = -10000.0;
        m_ub[2 + (PARAMS_PER_INTERFACE * i) + 2] = 10000.0;
    }
}

void LensSystemProblem::setRenderObjective(std::vector<SnapshotData> &renderObjective) {
    sortByQuadHeight(renderObjective);
    m_renderObjective = renderObjective;
}

SnapshotData LensSystemProblem::simulateDrawQuad(int quadId, glm::mat2x2& Ma, glm::mat2x2& Ms, float light_angle_x, float light_angle_y, float irisApertureHeight) const {

    //Projection of the aperture center onto the sensor
    glm::vec2 ghost_center_x = glm::vec2(-light_angle_x * Ma[1][0] / Ma[0][0], light_angle_x);
    glm::vec2 ghost_center_y = glm::vec2(-light_angle_y * Ma[1][0] / Ma[0][0], light_angle_y);

    glm::vec2 ghost_center_x_s = Ms * Ma * ghost_center_x;
    glm::vec2 ghost_center_y_s = Ms * Ma * ghost_center_y;
    glm::vec2 ghost_center_pos = glm::vec2(ghost_center_x_s.x, ghost_center_y_s.x);

    glm::vec2 apt_h_x = glm::vec2(((irisApertureHeight / 2.0) - (light_angle_x * Ma[1][0])) / Ma[0][0], light_angle_x);
    glm::vec2 apt_h_x_s = Ms * Ma * apt_h_x;
    float ghost_height = abs(apt_h_x_s.x - ghost_center_pos.x);

	glm::vec2 entrance_pupil_h_x_s = Ms * Ma * glm::vec2((m_entrance_pupil_height / 2.0), light_angle_x);
    glm::vec2 entrance_pupil_center_x_s = Ms * Ma * glm::vec2(0.f, light_angle_x);
    glm::vec2 entrance_pupil_center_y_s = Ms * Ma * glm::vec2(0.f, light_angle_y);
	float entrance_pupil_height = abs(entrance_pupil_h_x_s.x - entrance_pupil_center_x_s.x);
	glm::vec2 entrance_pupil_center_pos = glm::vec2(entrance_pupil_center_x_s.x, entrance_pupil_center_y_s.x);

    bool ghost_center_clipped = false;
    float dist_between_centers = glm::length(entrance_pupil_center_pos - ghost_center_pos);
    if (entrance_pupil_height < dist_between_centers && ghost_height < dist_between_centers) {
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
    for (int i = 0; i < m_num_interfaces; i++) {
        LensInterface lens;
        lens.di = dv[2 + (PARAMS_PER_INTERFACE * i)];
        lens.ni = dv[2 + (PARAMS_PER_INTERFACE * i) + 1];
        lens.Ri = dv[2 + (PARAMS_PER_INTERFACE * i) + 2];

		//Repair to realistic values
        if (lens.ni <= 1.25f) {
            lens.ni = 1.0f; //air gap
		}
        else {
            lens.ni += 0.25; //to bring it up to 1.5 - 2.0 range
        }

        if (lens.ni != 1.0f) { //glass interfaces are not thick, 1 - 10mm range
            lens.di = 1.0f + ((lens.di - 0.1f) / (100.0f - 0.1f)) * 9.f;
        }

        if (lens.Ri >= 0 && lens.Ri <= 5.0f) {
            lens.Ri = 5.0f;
        }
        else if (lens.Ri >= 8000.0) {
            lens.Ri = std::numeric_limits<float>::infinity();
        }
        else if (lens.Ri < 0 && lens.Ri >= -5.0f) {
            lens.Ri = -5.0f;
        }
        else if (lens.Ri <= -8000.0) {
            lens.Ri = -std::numeric_limits<float>::infinity();
        }

        newLensInterfaces.push_back(lens);
    }

    LensSystem newLensSystem = LensSystem(std::round(dv[0]), dv[1], m_entrance_pupil_height, newLensInterfaces);

    //"Render"
    glm::mat2x2 default_Ma = newLensSystem.getMa();
    glm::mat2x2 default_Ms = newLensSystem.getMs();
    std::vector<glm::vec2> preAptReflectionPairs = newLensSystem.getPreAptReflections();
    std::vector<glm::vec2> postAptReflectionPairs = newLensSystem.getPostAptReflections();

    // not enough ghosts, discard
	if (preAptReflectionPairs.size() + postAptReflectionPairs.size() < m_renderObjective.size()) {
		return { 100000.0 };
	}

    std::vector<glm::mat2x2> preAptMas = newLensSystem.getMa(preAptReflectionPairs);
    std::vector<glm::mat2x2> postAptMss = newLensSystem.getMs(postAptReflectionPairs);

    std::vector<SnapshotData> newSnapshot;
    
    for (int i = 0; i < preAptReflectionPairs.size(); i++) {
        newSnapshot.push_back(simulateDrawQuad(i, preAptMas[i], default_Ms, m_light_angle_x, m_light_angle_y, dv[1]));
    }
    for (int i = 0; i < postAptReflectionPairs.size(); i++) {
        newSnapshot.push_back(simulateDrawQuad(i + preAptReflectionPairs.size(), default_Ma, postAptMss[i], m_light_angle_x, m_light_angle_y, dv[1]));
    }

    // Compare ghosts on size (directly related to intensity)
    sortByQuadHeight(newSnapshot);

    //Compute fitness
    double f = 0.0;

    for (int i = 0; i < m_renderObjective.size(); i++) {
        float posError = glm::length(m_renderObjective[i].quadCenterPos - newSnapshot[i].quadCenterPos);
        float sizeError = m_renderObjective[i].quadHeight - newSnapshot[i].quadHeight;
		f += ((sizeError * sizeError) + (posError * posError)); //square because human perception more sensitive to big errors than small ones
    } 

    if (newSnapshot.size() > m_renderObjective.size()) {
        for (int i = m_renderObjective.size(); i < newSnapshot.size(); i++) {
			f += 500 / newSnapshot[i].quadHeight; // penalize extra ghosts proportional to their size
        }
    }

    f = f / m_renderObjective.size();

    return { f };
}

std::pair<pagmo::vector_double, pagmo::vector_double> LensSystemProblem::get_bounds() const {
    return { m_lb, m_ub };
}

std::string read_kernel_code(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file: " << filename << std::endl;
        std::exit(EXIT_FAILURE);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void LensSystemProblem::initializeOpenCL() const {
    if (m_clInitialized)
        return;

    // Get available platforms, pick one (for example, the first), then pick a GPU device
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    std::vector<cl::Device> devices;
    platforms[0].getDevices(CL_DEVICE_TYPE_GPU, &devices);
    if (devices.empty()) {
        throw std::runtime_error("No GPU devices found.");
    }
    m_clDevice = devices[0];
    m_clContext = cl::Context(m_clDevice);
    m_clQueue = cl::CommandQueue(m_clContext, m_clDevice);

    std::string kernel_filename = "batch_fitness.cl";
    std::string kernel_code = read_kernel_code(kernel_filename);

    cl::Program::Sources sources;
    sources.push_back({ kernel_code.c_str(), kernel_code.length() });
    m_clProgram = cl::Program(m_clContext, sources);

    // Build the program for the selected device.
    try {
        m_clProgram.build({ m_clDevice });
    }
    catch (const cl::Error& err) {
        std::cerr << "OpenCL Program Build Error: " << err.what() << "(" << err.err() << ")" << std::endl;
        std::cerr << m_clProgram.getBuildInfo<CL_PROGRAM_BUILD_LOG>(m_clDevice) << std::endl;
        throw;
    }

    m_clInitialized = true;
}

pagmo::vector_double LensSystemProblem::batch_fitness(const pagmo::vector_double& pop) const {
    // Ensure OpenCL is initialized
    initializeOpenCL();

    const int num_candidates = pop.size() / m_dim;
    const int candidate_dim = m_dim; // your dimension per candidate
    const int num_render_obj = m_renderObjective.size();

    // Pack population data into one contiguous vector.
    std::vector<double> h_population = pop;

    // Pack render objective data.
    // For this example, assume each render objective has three values: [center_x, center_y, quadHeight].
    std::vector<double> h_renderObj(num_render_obj * 3);
    for (int i = 0; i < num_render_obj; ++i) {
        // Assume m_renderObjective[i] contains quadCenterPos (x,y) and quadHeight.
        h_renderObj[i * 3 + 0] = m_renderObjective[i].quadCenterPos.x;
        h_renderObj[i * 3 + 1] = m_renderObjective[i].quadCenterPos.y;
        h_renderObj[i * 3 + 2] = m_renderObjective[i].quadHeight;
    }

    // Create OpenCL buffers.
    cl::Buffer d_population(m_clContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        sizeof(double) * h_population.size(), h_population.data());
    cl::Buffer d_renderObj(m_clContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        sizeof(double) * h_renderObj.size(), h_renderObj.data());
    cl::Buffer d_fitness(m_clContext, CL_MEM_WRITE_ONLY, sizeof(double) * num_candidates);

    // Create the kernel.
    cl::Kernel kernel(m_clProgram, "batch_fitness_kernel");

    // Set kernel arguments.
    int arg = 0;
    kernel.setArg(arg++, d_population);
    kernel.setArg(arg++, d_fitness);
    kernel.setArg(arg++, d_renderObj);
    kernel.setArg(arg++, candidate_dim);
    kernel.setArg(arg++, num_render_obj);
    kernel.setArg(arg++, m_light_angle_x);
    kernel.setArg(arg++, m_light_angle_y);

    // Launch the kernel.
    cl::NDRange global(num_candidates);
    try {
        m_clQueue.enqueueNDRangeKernel(kernel, cl::NullRange, global, cl::NullRange);
        m_clQueue.finish();
    }
    catch (const cl::Error& err) {
        std::cerr << "OpenCL Kernel Error: " << err.what() << "(" << err.err() << ")" << std::endl;
        throw;
    }

    // Read back the fitness results.
    std::vector<double> h_fitness(num_candidates);
    m_clQueue.enqueueReadBuffer(d_fitness, CL_TRUE, 0, sizeof(double) * num_candidates, h_fitness.data());

    // Convert to pagmo vector_double format.
    pagmo::vector_double pop_fitness;
    pop_fitness.reserve(num_candidates);
    for (int i = 0; i < num_candidates; ++i) {
        pop_fitness.push_back({ h_fitness[i] });
    }
    return pop_fitness;
}

pagmo::vector_double my_udbfe(const pagmo::problem& prob, const pagmo::vector_double& pop) {
	return prob.batch_fitness(pop);
}

//Convert a vector of LensInterface into a decision vector.
pagmo::vector_double convertLensSystem(unsigned int aptPos, const std::vector<LensInterface>& lens_system, float irisApertureHeight) {
    pagmo::vector_double decision;
    decision.reserve(2 + (lens_system.size() * PARAMS_PER_INTERFACE));
    decision.push_back(aptPos);
    decision.push_back(irisApertureHeight);
    for (const auto& lens : lens_system) {
        decision.push_back(lens.di);
        decision.push_back(lens.ni);
        decision.push_back(lens.Ri);
    }
    return decision;
}

void sortByQuadHeight(std::vector<SnapshotData>& snapshotDataUnsorted) {
    std::sort(snapshotDataUnsorted.begin(), snapshotDataUnsorted.end(),
        [](const SnapshotData& a, const SnapshotData& b) {
            return a.quadHeight < b.quadHeight;
        });
}

std::vector<std::vector<double>> runEA(pagmo::population pop,
    float light_angle_x,
    float light_angle_y,
    const std::string& filename,
    pagmo::algorithm algo) {
    // Open CSV log file.
    std::ofstream csvFile(filename, std::ios::app);
    if (!csvFile.is_open()) {
        std::cerr << "Error opening CSV log file!" << std::endl;
    }
    csvFile << "######################################################################" << std::endl;
    csvFile << "Light Angle X," << light_angle_x << std::endl;
    csvFile << "Light Angle Y," << light_angle_y << std::endl;
    csvFile << "Generation,Elapsed Time (sec),Total Evaluations,Best Fitness" << std::endl;

    // Get initial champion.
    std::vector<double> c_solution = pop.champion_x();
    double c_fitness = pop.champion_f()[0];
    std::cout << "Initial Best Fitness: " << c_fitness << std::endl;
    std::cout << "Initial Best decision vector: ";
    for (const double val : c_solution) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    unsigned long long total_fevals = 0;

    // Evolution loop for 40 generations.
    for (int gen = 0; gen < 10; ++gen) {
        std::cout << "EVOLVING GEN " << gen << std::endl;
        // Evolve the population using the provided algorithm.
        pop = algo.evolve(pop);

        // Retrieve the best (champion) fitness from the evolving population.
        double best_fitness = pop.champion_f()[0];
        std::cout << "CURRENT BEST FITNESS: " << best_fitness << std::endl;

        // Get the total function evaluations using the problem's get_fevals() method.
        total_fevals = pop.get_problem().get_fevals();
        std::cout << "Total function evaluations after gen " << gen << ": " << total_fevals << std::endl;

        // Log generation details.
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed_secs = std::chrono::duration_cast<std::chrono::seconds>(currentTime - start).count();
        csvFile << gen << ","
            << elapsed_secs << ","
            << total_fevals << ","
            << best_fitness << std::endl;
    }

    // Final time computations.
    auto end = std::chrono::high_resolution_clock::now();
    auto total_elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    int minutes = static_cast<int>(total_elapsed_sec / 60);
    int seconds = static_cast<int>(total_elapsed_sec % 60);
    std::cout << "Computation time: " << minutes << " minutes and " << seconds << " seconds" << std::endl;

    csvFile << std::endl;
    csvFile << "Final Computation Time (min:sec):," << minutes << ":" << seconds << std::endl;
    csvFile << "Total Function Evaluations:," << total_fevals << std::endl;

    // Gather all individuals in the population and sort them by fitness.
    auto xs = pop.get_x();
    auto fs = pop.get_f();
    std::vector<std::pair<double, std::vector<double>>> champions;
    for (size_t i = 0; i < fs.size(); ++i) {
        champions.emplace_back(fs[i][0], xs[i]);
    }
    std::sort(champions.begin(), champions.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    // Log and output the top 5 champions.
    std::cout << "\nTop 5 Champions:" << std::endl;
    std::vector<std::vector<double>> top5;
    size_t num = std::min(champions.size(), static_cast<size_t>(5));
    csvFile << std::endl;
    csvFile << "Champion Rank,Best Fitness,Decision Vector" << std::endl;
    for (size_t i = 0; i < num; ++i) {
        std::cout << "Fitness " << champions[i].first << ": ";
        std::ostringstream decision_vector;
        for (const auto& val : champions[i].second) {
            std::cout << val << " ";
            decision_vector << val << " ";
        }
        std::cout << std::endl;
        csvFile << (i + 1) << "," << champions[i].first << ",\"" << decision_vector.str() << "\"" << std::endl;
        top5.push_back(champions[i].second);
    }

    if (!champions.empty()) {
        csvFile << std::endl;
        csvFile << "Final Best Champion,"
            << champions[0].first << ",\"";
        for (const auto& val : champions[0].second) {
            csvFile << val << " ";
        }
        csvFile << "\"" << std::endl;
    }
    csvFile.close();

    // Return the decision vectors of the top 5 champions.
    return top5;
}


std::vector<LensSystem> solveLensAnnotations(LensSystem& currentLensSystem,
    std::vector<SnapshotData>& renderObjective,
    float light_angle_x,
    float light_angle_y) {
    // Retrieve current lens interfaces and the number of interfaces.
    std::vector<LensInterface> currentLensInterfaces = currentLensSystem.getLensInterfaces();
    unsigned int num_interfaces = currentLensInterfaces.size();

    // Set up the pagmo problem.
    LensSystemProblem my_problem;
    my_problem.init(num_interfaces, light_angle_x, light_angle_y);
    my_problem.setRenderObjective(renderObjective);
    pagmo::problem prob{ my_problem };
    
    std::cout << "Created Pagmo UDP" << prob.has_batch_fitness() << std::endl;

    // Convert the current lens system to a decision vector.
    std::vector<double> current_point = convertLensSystem(currentLensSystem.getIrisAperturePos(),
        currentLensInterfaces,
        currentLensSystem.getApertureHeight());

    // Set up the evolutionary algorithm.
    //pagmo::algorithm algo{ pagmo::sade(200u, true )};
    //pagmo::algorithm algo{ pagmo::pso(200u) };
    //pagmo::algorithm algo{ pagmo::de1220{100} };
    //pagmo::algorithm algo{ pagmo::cmaes(200) };
    //pagmo::algorithm algo{ pagmo::gaco{200} };
    //pagmo::algorithm algo{ pagmo::bee_colony{200} };

    std::vector<unsigned> seeds = {100, 200, 300, 400, 500, 600, 700, 800, 900, 4747, 6969};
     
    unsigned int amount_dv = current_point.size();
    std::vector<std::vector<double>> top5_decision_vectors;

    pagmo::bfe my_bfe(my_udbfe);
    pagmo::pso_gen pso_geny(200u);
	pso_geny.set_bfe(my_bfe);
    pagmo::algorithm algo{ pso_geny };
    pagmo::population pop(prob, my_bfe, 500 * amount_dv);

    top5_decision_vectors = runEA(pop, light_angle_x, light_angle_y, "pso_gen_gpu.csv", algo);


    std::vector<LensSystem> top5_lens_systems;
    for (const auto& decision_vector : top5_decision_vectors) {
        // Construct the new lens interfaces for this candidate.
        std::vector<LensInterface> optimized_lens_system;
        for (int i = 0; i < static_cast<int>(num_interfaces); i++) {
            LensInterface lens;
            lens.di = decision_vector[2 + (PARAMS_PER_INTERFACE * i)];
            lens.ni = decision_vector[2 + (PARAMS_PER_INTERFACE * i) + 1];
            lens.Ri = decision_vector[2 + (PARAMS_PER_INTERFACE * i) + 2];

            //Repair to realistic values
            if (lens.ni <= 1.25f) {
                lens.ni = 1.0f; //air gap
            }
            else {
                lens.ni += 0.25; //to bring it up to 1.5 - 2.0 range
            }

            if (lens.ni != 1.0f) { //glass interfaces are not thick, 1 - 10mm range
                lens.di = 1.0f + ((lens.di - 0.1f) / (100.0f - 0.1f)) * 9.f;
            }

            if (lens.Ri >= 0 && lens.Ri <= 5.0f) {
                lens.Ri = 5.0f;
            }
            else if (lens.Ri >= 8000.0) {
                lens.Ri = std::numeric_limits<float>::infinity();
            }
            else if (lens.Ri < 0 && lens.Ri >= -5.0f) {
                lens.Ri = -5.0f;
            }
            else if (lens.Ri <= -8000.0) {
                lens.Ri = -std::numeric_limits<float>::infinity();
            }

            // Use the original lambda0 from the current lens interface.
            lens.lambda0 = currentLensInterfaces[i].lambda0;
            optimized_lens_system.push_back(lens);
        }

        // Construct the LensSystem.
        LensSystem candidate(std::round(decision_vector[0]),
            decision_vector[1],
            100.f,
            optimized_lens_system);
        top5_lens_systems.push_back(candidate);
    }

    for (size_t j = 0; j < top5_lens_systems.size(); j++) {
        std::cout << "\nTop " << (j + 1) << " Lens System:" << std::endl;
        std::vector<LensInterface> interfaces = top5_lens_systems[j].getLensInterfaces();
        for (size_t i = 0; i < interfaces.size(); i++) {
            std::cout << "Interface " << i << ": "
                << "di = " << interfaces[i].di << ", "
                << "ni = " << interfaces[i].ni << ", "
                << "Ri = " << interfaces[i].Ri << ", "
                << "lambda0 = " << interfaces[i].lambda0 << std::endl;
        }
    }

    // Return the top 5 optimized lens systems.
    return top5_lens_systems;
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

std::vector<LensSystem> solveLensAnnotations(std::vector<SnapshotData>& renderObjective,
    float light_angle_x,
    float light_angle_y) {

    unsigned int num_interfaces = interfacesNeeded(renderObjective.size());

    LensSystemProblem my_problem;
    my_problem.init(num_interfaces, light_angle_x, light_angle_y);
    my_problem.setRenderObjective(renderObjective);
    pagmo::problem prob{ my_problem };
    std::cout << "Created Pagmo UDP" << std::endl;

    //pagmo::algorithm algo{ pagmo::pso{200} };

    unsigned int amount_dv = 2 + (num_interfaces * PARAMS_PER_INTERFACE);

    pagmo::bfe my_bfe(my_udbfe);
    pagmo::pso_gen pso_geny(200u);
    pso_geny.set_bfe(my_bfe);
    pagmo::algorithm algo{ pso_geny };
    pagmo::population pop(prob, my_bfe, 200 * amount_dv);

    std::vector<std::vector<double>> top5_champions = runEA(pop, light_angle_x, light_angle_y, "pso_gen_gpu.csv", algo);

    std::vector<LensSystem> top5_lens_systems;
    for (const auto& candidate : top5_champions) {
        std::vector<LensInterface> optimized_lens_system;
        for (int i = 0; i < static_cast<int>(num_interfaces); i++) {
            LensInterface lens;
            lens.di = candidate[2 + (PARAMS_PER_INTERFACE * i)];
            lens.ni = candidate[2 + (PARAMS_PER_INTERFACE * i) + 1];
            lens.Ri = candidate[2 + (PARAMS_PER_INTERFACE * i) + 2];

            //Repair to realistic values
            if (lens.ni <= 1.25f) {
                lens.ni = 1.0f; //air gap
            }
            else {
                lens.ni += 0.25; //to bring it up to 1.5 - 2.0 range
            }

            if (lens.ni != 1.0f) { //glass interfaces are not thick, 1 - 10mm range
                lens.di = 1.0f + ((lens.di - 0.1f) / (100.0f - 0.1f)) * 9.f;
            }

            if (lens.Ri >= 0) {
                lens.Ri = std::clamp(lens.Ri, 5.0f, 1000.0f);
            }
            else if (lens.Ri < 0) {
                lens.Ri = std::clamp(lens.Ri, -1000.0f, -5.0f);
            }

            lens.lambda0 = 440;
            optimized_lens_system.push_back(lens);
        }

        LensSystem candidate_ls(std::round(candidate[0]),
            candidate[1],
            100,
            optimized_lens_system);
        top5_lens_systems.push_back(candidate_ls);
    }

    for (size_t j = 0; j < top5_lens_systems.size(); j++) {
        std::cout << "\nTop " << (j + 1) << " Lens System:" << std::endl;
        std::vector<LensInterface> interfaces = top5_lens_systems[j].getLensInterfaces();
        for (size_t i = 0; i < interfaces.size(); i++) {
            std::cout << "Interface " << i << ": "
                << "di = " << interfaces[i].di << ", "
                << "ni = " << interfaces[i].ni << ", "
                << "Ri = " << interfaces[i].Ri << ", "
                << "lambda0 = " << interfaces[i].lambda0 << std::endl;
        }
    }

    return top5_lens_systems;
}
