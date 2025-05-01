#include "reverse_coating.h"

#include <glm/glm.hpp>
#include <vector>
#include <cmath>
#include <numbers>
#include <iostream>
#include "utils.h"

//float RED_WAVELENGTH = 650;
//float GREEN_WAVELENGTH = 510;
//float BLUE_WAVELENGTH = 475;


void optimizeLensCoatingsGridSearch(LensSystem& lensSystem, glm::vec3 desiredColor, glm::vec2 reflectionPair, glm::vec2 yawAndPitch) {
    std::vector<LensInterface> lensInterfaces = lensSystem.getLensInterfaces();
    std::vector<glm::vec2> incident_angles = lensSystem.getPathIncidentAngleAtReflectionPos(reflectionPair, yawAndPitch);

    float first_n1 = std::max(sqrt(lensInterfaces[reflectionPair.x - 1].ni * lensInterfaces[reflectionPair.x].ni), 1.38f);
    float second_n1 = reflectionPair.y == 0
        ? std::max(sqrt(lensInterfaces[reflectionPair.y].ni), 1.38f)
        : std::max(sqrt(lensInterfaces[reflectionPair.y - 1].ni * lensInterfaces[reflectionPair.y].ni), 1.38f);

    float minlambda = 380.0f;
    float maxlambda = 740.0f;

    float bestError = std::numeric_limits<float>::max();
    float bestLambda1 = lensInterfaces[reflectionPair.x].lambda0;
    float bestLambda2 = lensInterfaces[reflectionPair.y].lambda0;

    std::cout << "START GRID SEARCH" << std::endl;

    for (float candidateLambda1 = minlambda; candidateLambda1 < maxlambda; candidateLambda1 += 1.0f) {

        float thickness1 = candidateLambda1 / 4.0f / first_n1;

        glm::vec3 reflectivity1 =
            lensSystem.computeFresnelAR(incident_angles[0].x, thickness1, lensInterfaces[reflectionPair.x - 1].ni, first_n1, lensInterfaces[reflectionPair.x].ni) +
            lensSystem.computeFresnelAR(incident_angles[0].y, thickness1, lensInterfaces[reflectionPair.x - 1].ni, first_n1, lensInterfaces[reflectionPair.x].ni);


        for (float candidateLambda2 = minlambda; candidateLambda2 < maxlambda; candidateLambda2 += 2.0f) {

            float thickness2 = candidateLambda2 / 4.0f / second_n1;
            glm::vec3 reflectivity2;

            if (reflectionPair.y == 0) {
                reflectivity2 =
                    lensSystem.computeFresnelAR(incident_angles[1].x, thickness2, lensInterfaces[reflectionPair.y].ni, second_n1, 1.0f) +
                    lensSystem.computeFresnelAR(incident_angles[1].y, thickness2, lensInterfaces[reflectionPair.y].ni, second_n1, 1.0f);
            }
            else {
                reflectivity2 =
                    lensSystem.computeFresnelAR(incident_angles[1].x, thickness2, lensInterfaces[reflectionPair.y].ni, second_n1, lensInterfaces[reflectionPair.y - 1].ni) +
                    lensSystem.computeFresnelAR(incident_angles[1].y, thickness2, lensInterfaces[reflectionPair.y].ni, second_n1, lensInterfaces[reflectionPair.y - 1].ni);
            }

            glm::vec3 combinedReflectivity = reflectivity1 * reflectivity2;

            glm::vec3 normCombined = normalizeRGB(combinedReflectivity);
            glm::vec3 normDesired = normalizeRGB(desiredColor);
            float error = glm::length(normCombined - normDesired);

            if (error < bestError) {
                bestError = error;
                bestLambda1 = candidateLambda1;
                bestLambda2 = candidateLambda2;
            }
        }
    }

    std::cout << "GRID SEARCH FINISHED" << std::endl;
    std::cout << "Best combined error: " << bestError << std::endl;
    std::cout << "Optimal lambda for first interface: " << bestLambda1 << " nm" << std::endl;
    std::cout << "Optimal lambda for second interface: " << bestLambda2 << " nm" << std::endl;

    // Update the lens interfaces with the optimized coating wavelengths.
    lensInterfaces[reflectionPair.x].lambda0 = bestLambda1;
    lensInterfaces[reflectionPair.y].lambda0 = bestLambda2;
    lensSystem.setLensInterfaces(lensInterfaces);
}

std::pair<std::vector<std::pair<float, glm::vec3>>, std::vector<std::pair<float, glm::vec3>>> computeReflectivityPerLambda(LensSystem& lensSystem, glm::vec2 reflectionPair, glm::vec2 yawAndPitch) {
    std::vector<LensInterface> lensInterfaces = lensSystem.getLensInterfaces();
    std::vector<glm::vec2> incident_angles = lensSystem.getPathIncidentAngleAtReflectionPos(reflectionPair, glm::vec2(0.0001));

    float first_n1 = std::max(sqrt(lensInterfaces[reflectionPair.x - 1].ni * lensInterfaces[reflectionPair.x].ni), 1.38f);
    float second_n1 = reflectionPair.y == 0
        ? std::max(sqrt(lensInterfaces[reflectionPair.y].ni), 1.38f)
        : std::max(sqrt(lensInterfaces[reflectionPair.y - 1].ni * lensInterfaces[reflectionPair.y].ni), 1.38f);

    std::vector<std::pair<float, glm::vec3>> firstReflectivityData;
    std::vector<std::pair<float, glm::vec3>> secondReflectivityData;

    for (float i_lambda = 380; i_lambda <= 740; i_lambda += 4) {
        float first_d1 = i_lambda / 4 / first_n1;
        float second_d1 = i_lambda / 4 / second_n1;

        glm::vec3 firstReflectivity = lensSystem.computeFresnelAR(
            incident_angles[0].x, first_d1, lensInterfaces[reflectionPair.x - 1].ni, first_n1, lensInterfaces[reflectionPair.x].ni
        ) + lensSystem.computeFresnelAR(
            incident_angles[0].y, first_d1, lensInterfaces[reflectionPair.x - 1].ni, first_n1, lensInterfaces[reflectionPair.x].ni
        );

        glm::vec3 secondReflectivity = reflectionPair.y == 0
            ? lensSystem.computeFresnelAR(
                incident_angles[1].x, second_d1, lensInterfaces[reflectionPair.y].ni, second_n1, 1.f
            ) + lensSystem.computeFresnelAR(
                incident_angles[1].y, second_d1, lensInterfaces[reflectionPair.y].ni, second_n1, 1.f
            )
            : lensSystem.computeFresnelAR(
                incident_angles[1].x, second_d1, lensInterfaces[reflectionPair.y].ni, second_n1, lensInterfaces[reflectionPair.y - 1].ni
            ) + lensSystem.computeFresnelAR(
                incident_angles[1].y, second_d1, lensInterfaces[reflectionPair.y].ni, second_n1, lensInterfaces[reflectionPair.y - 1].ni
            );

        firstReflectivityData.push_back({ i_lambda, firstReflectivity });
        secondReflectivityData.push_back({ i_lambda, secondReflectivity });
    }

    return { firstReflectivityData, secondReflectivityData };
}



