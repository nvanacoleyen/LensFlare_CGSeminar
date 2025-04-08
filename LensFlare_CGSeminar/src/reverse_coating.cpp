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

void optimizeLensCoatingsSimple(LensSystem& lensSystem, glm::vec3 color, glm::vec2 reflectionPair) {

    std::vector<LensInterface> lensInterfaces = lensSystem.getLensInterfaces();

    lensInterfaces[reflectionPair.x].lambda0 = rgbToWavelength(color.x, color.y, color.z);
    lensInterfaces[reflectionPair.y].lambda0 = rgbToWavelength(color.x, color.y, color.z);

    lensSystem.setLensInterfaces(lensInterfaces);

}

void optimizeLensCoatingsBruteForce(LensSystem& lensSystem, glm::vec3 desiredColor, glm::vec2 reflectionPair, glm::vec2 yawAndPitch) {

    std::vector<LensInterface> lensInterfaces = lensSystem.getLensInterfaces();
    std::vector<glm::vec2> incident_angles = lensSystem.getPathIncidentAngleAtReflectionPos(reflectionPair, yawAndPitch);

    float first_n1 = std::max(sqrt(lensInterfaces[reflectionPair.x - 1].ni * lensInterfaces[reflectionPair.x].ni), 1.38f);
    float first_d1 = lensInterfaces[reflectionPair.x].lambda0 / 4 / first_n1;
    float second_n1;
    if (reflectionPair.y == 0) {
        second_n1 = std::max(sqrt(lensInterfaces[reflectionPair.y].ni), 1.38f);
    }
    else {
        second_n1 = std::max(sqrt(lensInterfaces[reflectionPair.y - 1].ni * lensInterfaces[reflectionPair.y].ni), 1.38f);
    }
    float second_d1 = lensInterfaces[reflectionPair.y].lambda0 / 4 / second_n1;

    glm::vec3 interfaceReflectivity = lensSystem.computeFresnelAR(incident_angles[0].x, first_d1, lensInterfaces[reflectionPair.x - 1].ni, first_n1, lensInterfaces[reflectionPair.x].ni) + lensSystem.computeFresnelAR(incident_angles[0].y, first_d1, lensInterfaces[reflectionPair.x - 1].ni, first_n1, lensInterfaces[reflectionPair.x].ni);
    float minError1 = glm::length(desiredColor - interfaceReflectivity);
    float minError2;
    if (reflectionPair.y == 0) {
        minError2 = glm::length(desiredColor - (lensSystem.computeFresnelAR(incident_angles[1].x, second_d1, lensInterfaces[reflectionPair.y].ni, second_n1, 1.f) + lensSystem.computeFresnelAR(incident_angles[1].y, second_d1, lensInterfaces[reflectionPair.y].ni, second_n1, 1.f)));
    }
    else {
        minError2 = glm::length(desiredColor - (lensSystem.computeFresnelAR(incident_angles[1].x, second_d1, lensInterfaces[reflectionPair.y].ni, second_n1, lensInterfaces[reflectionPair.y - 1].ni) + lensSystem.computeFresnelAR(incident_angles[1].y, second_d1, lensInterfaces[reflectionPair.y].ni, second_n1, lensInterfaces[reflectionPair.y - 1].ni)));
    }
    float minFirstLambda0 = lensInterfaces[reflectionPair.x].lambda0;
    float minSecondLambda0 = lensInterfaces[reflectionPair.y].lambda0;

    //visible spectrum
    /*float minlambda = 350;
    float maxlambda = 800;*/
    float minlambda = 1;
    float maxlambda = 10000;

    LensInterface firstInterface = lensInterfaces[reflectionPair.x];
    LensInterface secondInterface = lensInterfaces[reflectionPair.y];

    std::cout << "START BRUTE FORCE" << std::endl;

    for (float i_lambda = minlambda; i_lambda < maxlambda; i_lambda++) {

        //SET NEW 
        first_d1 = i_lambda / 4 / first_n1;
        second_d1 = i_lambda / 4 / second_n1;

        glm::vec3 first_newReflectivity = lensSystem.computeFresnelAR(incident_angles[0].x, first_d1, lensInterfaces[reflectionPair.x - 1].ni, first_n1, lensInterfaces[reflectionPair.x].ni) + lensSystem.computeFresnelAR(incident_angles[0].y, first_d1, lensInterfaces[reflectionPair.x - 1].ni, first_n1, lensInterfaces[reflectionPair.x].ni);
        glm::vec3 second_newReflectivity;
        if (reflectionPair.y == 0) {
            second_newReflectivity = lensSystem.computeFresnelAR(incident_angles[1].x, second_d1, lensInterfaces[reflectionPair.y].ni, second_n1, 1.f) + lensSystem.computeFresnelAR(incident_angles[1].y, second_d1, lensInterfaces[reflectionPair.y].ni, second_n1, 1.f);
        }
        else {
            second_newReflectivity = lensSystem.computeFresnelAR(incident_angles[1].x, second_d1, lensInterfaces[reflectionPair.y].ni, second_n1, lensInterfaces[reflectionPair.y - 1].ni) + lensSystem.computeFresnelAR(incident_angles[1].y, second_d1, lensInterfaces[reflectionPair.y].ni, second_n1, lensInterfaces[reflectionPair.y - 1].ni);
        }

        // Compute error
        float newError1 = glm::length(desiredColor - first_newReflectivity);
        float newError2 = glm::length(desiredColor - second_newReflectivity);
        //std::cout << "ERROR " << glm::length(error) << std::endl;

        if (newError1 < minError1) {
            minError1 = newError1;
            minFirstLambda0 = i_lambda;
        }
        if (newError2 < minError2) {
            minError2 = newError2;
            minSecondLambda0 = i_lambda;
        }
    }

    std::cout << "BRUTE FORCE FINISHED" << std::endl;
    std::cout << "Best error was: " << minError1 << std::endl;
    std::cout << "Best error was: " << minError2 << std::endl;
    lensInterfaces[reflectionPair.x].lambda0 = minFirstLambda0;
    lensInterfaces[reflectionPair.y].lambda0 = minSecondLambda0;
    lensSystem.setLensInterfaces(lensInterfaces);

}


