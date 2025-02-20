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

void optimizeLensCoatings(LensSystem& lensSystem, glm::vec3 desiredColor, glm::vec2 reflectionPair, glm::vec2 yawAndPitch) {

    std::vector<LensInterface> lensInterfaces = lensSystem.getLensInterfaces();

    int nIter = 1000;
    float learning_Rate = 1;

    glm::vec3 lightIntensity(50.f);

    for (int i = 0; i < nIter; i++) {

        glm::vec3 newReflectivity = lensSystem.propagateTransmission(reflectionPair.x, reflectionPair.y, glm::vec2(0.0f, yawAndPitch.x)) + lensSystem.propagateTransmission(reflectionPair.x, reflectionPair.y, glm::vec2(0.0f, yawAndPitch.y));
        glm::vec3 error = desiredColor - (lightIntensity * newReflectivity);

        std::cout << glm::length(error) << std::endl;
        if (glm::length(error) < 0.75f) {  // Convergence criterion
            std::cout << "Algorithm Converged" << std::endl;
            break;
        }

        if (error.x > error.y && error.x > error.z) { //R
            lensInterfaces[reflectionPair.x].lambda0 += learning_Rate * error.x;
            lensInterfaces[reflectionPair.y].lambda0 += learning_Rate * error.x;
        }
        else if (error.y > error.x && error.y > error.z) { //G
            if (lensInterfaces[reflectionPair.x].lambda0 > 550) {
                lensInterfaces[reflectionPair.x].lambda0 -= learning_Rate * error.y;
            }
            else {
                lensInterfaces[reflectionPair.x].lambda0 += learning_Rate * error.y;
            }
            if (lensInterfaces[reflectionPair.y].lambda0 > 550) {
                lensInterfaces[reflectionPair.y].lambda0 -= learning_Rate * error.y;
            }
            else {
                lensInterfaces[reflectionPair.y].lambda0 += learning_Rate * error.y;
            }
        }
        else { //B
            lensInterfaces[reflectionPair.x].lambda0 -= learning_Rate * error.z;
            lensInterfaces[reflectionPair.y].lambda0 -= learning_Rate * error.z;
        }

        lensSystem.setLensInterfaces(lensInterfaces);

    }

}

void optimizeLensCoatingsBruteForce(LensSystem& lensSystem, glm::vec3 desiredColor, glm::vec2 reflectionPair, glm::vec2 yawAndPitch) {

    std::vector<LensInterface> lensInterfaces = lensSystem.getLensInterfaces();
    glm::vec3 lightIntensity(50.f);
    float minError = glm::length(desiredColor - (lightIntensity * lensSystem.propagateTransmission(reflectionPair.x, reflectionPair.y, glm::vec2(0.0f, yawAndPitch.x)) + lensSystem.propagateTransmission(reflectionPair.x, reflectionPair.y, glm::vec2(0.0f, yawAndPitch.y))));
    float minFirstLambda0 = lensInterfaces[reflectionPair.x].lambda0;
    float minSecondLambda0 = lensInterfaces[reflectionPair.y].lambda0;
    
    //visible spectrum
    float minlambda = 350;
    float maxlambda = 800;

    std::cout << "START BRUTE FORCE" << std::endl;

    for (float firstLambda0 = minlambda; firstLambda0 < maxlambda; firstLambda0++) {
        for (float secondLambda0 = minlambda; secondLambda0 < maxlambda; secondLambda0++) {
            lensInterfaces[reflectionPair.x].lambda0 = firstLambda0;
            lensInterfaces[reflectionPair.y].lambda0 = secondLambda0;
            lensSystem.setLensInterfaces(lensInterfaces);
            glm::vec3 newReflectivity = lensSystem.propagateTransmission(reflectionPair.x, reflectionPair.y, glm::vec2(0.0f, yawAndPitch.x)) + lensSystem.propagateTransmission(reflectionPair.x, reflectionPair.y, glm::vec2(0.0f, yawAndPitch.y));
            float newError = glm::length(desiredColor - (lightIntensity * newReflectivity));
            if (newError < minError) {
                minError = newError;
                minFirstLambda0 = firstLambda0;
                minSecondLambda0 = secondLambda0;
            }
        }
    }


    std::cout << "BRUTE FORCE FINISHED" << std::endl;
    std::cout << "Best error was: " << minError << std::endl;
    lensInterfaces[reflectionPair.x].lambda0 = minFirstLambda0;
    lensInterfaces[reflectionPair.y].lambda0 = minSecondLambda0;
    lensSystem.setLensInterfaces(lensInterfaces);


}

void optimizeLensCoatingsSimple(LensSystem& lensSystem, glm::vec3 color, glm::vec2 reflectionPair) {

    std::vector<LensInterface> lensInterfaces = lensSystem.getLensInterfaces();

    lensInterfaces[reflectionPair.x].lambda0 = rgbToWavelength(color.x, color.y, color.z);
    lensInterfaces[reflectionPair.y].lambda0 = rgbToWavelength(color.x, color.y, color.z);

    lensSystem.setLensInterfaces(lensInterfaces);

}



//////////////////


// Maximum number of iterations and tolerance for convergence
const int MAX_ITERATIONS = 10000;
const float TOLERANCE = 0.6;

// New method to compute thickness for desired reflectivities
float computeThicknessForDesiredReflectivity(
    float theta0,            // Angle of incidence
    float n0,                // RI of 1st medium
    float n1,                // RI of coating layer
    float n2,                // RI of the 2nd medium
    const glm::vec3& desiredReflectivity,  // Desired reflectivity (RGB)
    LensSystem& lensSystem
) {
    
    // Initial guess for thickness (in meters)
    float d1 = (550) / (4 * n1); // Start with quarter-wave thickness at green

    float stepSize = 5; // Step size for numerical derivative (1 nanometer)

    for (int i = 0; i < MAX_ITERATIONS; ++i) {
        // Compute current reflectivity
        glm::vec3 currentReflectivity = lensSystem.computeFresnelAR(theta0, d1, n0, n1, n2);

        // Compute error
        glm::vec3 error = currentReflectivity - desiredReflectivity;
        std::cout << "ERROR " << glm::length(error) << std::endl;
        // Check for convergence
        if (glm::length(error) < TOLERANCE) {
            std::cout << "Converged after " << i + 1 << " iterations.\n";
            return d1;
        }

        // Numerical derivative (gradient approximation)
        float d1_plus = d1 + stepSize;
        glm::vec3 reflectivityPlus = lensSystem.computeFresnelAR(theta0, d1_plus, n0, n1, n2);
        glm::vec3 derivative = ((reflectivityPlus - currentReflectivity) / stepSize) + 1e-6f;

        // Update thickness using Gradient Descent
        float learningRate = 0.01; // Adjust learning rate as needed
        glm::vec3 gradient = error / derivative;

        // Average gradient components to get scalar adjustment
        float avgGradient = (gradient.r + gradient.g + gradient.b) / 3.0f;

        // Update thickness
        d1 -= learningRate * avgGradient;

        // Ensure thickness remains positive
        d1 = std::max(d1, 0.0f);
        std::cout << "NEW D1: " << d1 << std::endl;
    }

    std::cerr << "Did not converge within the maximum number of iterations.\n";
    return d1; // Indicates failure
}

void optimizeLensCoatingsGradientDescent(LensSystem& lensSystem, glm::vec3 desiredColor, glm::vec2 reflectionPair, glm::vec2 yawAndPitch) {

    std::vector<LensInterface> lensInterfaces = lensSystem.getLensInterfaces();
    LensInterface firstInterface = lensInterfaces[reflectionPair.x];
    LensInterface secondInterface = lensInterfaces[reflectionPair.y];

    // Compute thickness
    float first_n1 = std::max(sqrt(lensInterfaces[reflectionPair.x - 1].ni * lensInterfaces[reflectionPair.x].ni), 1.38f);
    float first_d1 = computeThicknessForDesiredReflectivity(0.03, lensInterfaces[reflectionPair.x - 1].ni, first_n1, lensInterfaces[reflectionPair.x].ni, desiredColor, lensSystem);

    float second_n1 = std::max(sqrt(lensInterfaces[reflectionPair.y - 1].ni * lensInterfaces[reflectionPair.y].ni), 1.38f);
    float second_d1 = computeThicknessForDesiredReflectivity(0.03, lensInterfaces[reflectionPair.y].ni, second_n1, lensInterfaces[reflectionPair.y - 1].ni, desiredColor, lensSystem);


    std::cout << "Computed first d1: " << first_d1 << " nm\n";
    std::cout << "Computed second 2 d1: " << second_d1 << " nm\n";

    float first_lambda0 = first_d1 * first_n1 * 4;
    float second_lambda0 = second_d1 * second_n1 * 4;

    std::cout << "Computed first lambda0: " << first_lambda0 << " nm\n";
    std::cout << "Computed second lambda0: " << second_lambda0 << " nm\n";

    lensInterfaces[reflectionPair.x].lambda0 = first_lambda0;
    lensInterfaces[reflectionPair.y].lambda0 = second_lambda0;

    lensSystem.setLensInterfaces(lensInterfaces);

}

void optimizeLensCoatingsGradientDescent2(LensSystem& lensSystem, glm::vec3 desiredColor, glm::vec2 reflectionPair, glm::vec2 yawAndPitch) {
    std::vector<LensInterface> lensInterfaces = lensSystem.getLensInterfaces();

    int nIter = 1000;
    float learningRate = 10000000.f; // Adjust the learning rate as needed
    glm::vec3 lightIntensity(50.f);

    for (int i = 0; i < nIter; i++) {
        // Compute new reflectivity
        glm::vec3 newReflectivity = lensSystem.propagateTransmission(reflectionPair.x, reflectionPair.y, glm::vec2(0.0f, yawAndPitch.x)) +
            lensSystem.propagateTransmission(reflectionPair.x, reflectionPair.y, glm::vec2(0.0f, yawAndPitch.y));

        // Compute error
        glm::vec3 error = desiredColor - (lightIntensity * newReflectivity);

        std::cout << glm::length(error) << std::endl;
        if (glm::length(error) < 0.6f) {  // Convergence criterion
            std::cout << "Algorithm Converged" << std::endl;
            break;
        }

        // Compute gradients
        glm::vec3 gradient(0.0f);
        glm::vec3 reflectivityOriginal = newReflectivity;

        for (int j = 0; j < 2; j++) {
            float originalLambda0 = lensInterfaces[reflectionPair[j]].lambda0;

            // Small change to compute gradient
            float delta = 2.f;
            lensInterfaces[reflectionPair[j]].lambda0 = originalLambda0 + delta;

            // Compute reflectivity with perturbed parameter
            lensSystem.setLensInterfaces(lensInterfaces);
            glm::vec3 reflectivityPerturbed = lensSystem.propagateTransmission(reflectionPair.x, reflectionPair.y, glm::vec2(0.0f, yawAndPitch.x)) +
                lensSystem.propagateTransmission(reflectionPair.x, reflectionPair.y, glm::vec2(0.0f, yawAndPitch.y));

            // Compute gradient
            gradient[j] = (glm::length((reflectivityPerturbed - reflectivityOriginal)) ) / delta;

            // Restore original lambda0
            lensInterfaces[reflectionPair[j]].lambda0 = originalLambda0;
        }

        // Update lambda0 using gradient descent
        for (int j = 0; j < 2; j++) {
            lensInterfaces[reflectionPair[j]].lambda0 -= learningRate * error[j] * gradient[j];
            std::cout << lensInterfaces[reflectionPair[j]].lambda0 << std::endl;
        }

        lensSystem.setLensInterfaces(lensInterfaces);
    }
}

void optimizeLensCoatingsBruteForce2(LensSystem& lensSystem, glm::vec3 desiredColor, glm::vec2 reflectionPair, glm::vec2 yawAndPitch) {

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


