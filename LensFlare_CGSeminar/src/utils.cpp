#include "utils.h"

#include <algorithm>
#include <iostream>

// Function to translate a point to the camera plane
glm::vec3 translateToCameraSpace(const glm::vec3& cameraPos, const glm::vec3& cameraForward, const glm::vec3& cameraUp, const glm::vec3& point) {
    glm::vec3 toPoint = point - cameraPos;
    glm::vec3 right = glm::normalize(glm::cross(cameraForward, cameraUp));

    glm::vec3 cameraSpacePoint(
        glm::dot(toPoint, right),
        glm::dot(toPoint, cameraUp),
        glm::dot(toPoint, cameraForward)
    );

    return cameraSpacePoint;
}

glm::vec2 getYawandPitch(const glm::vec3& cameraPos, const glm::vec3& cameraForward, const glm::vec3& cameraUp, const glm::vec3& lightPos) {

    glm::vec3 cameraSpacePoint = translateToCameraSpace(cameraPos, cameraForward, cameraUp, lightPos);
    // Calculate yaw (angle around the y-axis) and pitch (angle around the x-axis)
    float yaw = -atan2(cameraSpacePoint.x, cameraSpacePoint.z);
    float pitch = atan2(cameraSpacePoint.y, cameraSpacePoint.z);

    //std::cout << "Yaw: " << yaw << ", Pitch: " << pitch << std::endl;

    return glm::vec2(yaw, pitch);
}

//CHECK ACCURACY
glm::vec3 wavelengthToRGB(float wavelength) {
    float R = 0.0, G = 0.0, B = 0.0;

    if (wavelength >= 380.0 && wavelength <= 440.0) {
        R = -1.0 * (wavelength - 440.0) / (440.0 - 380.0);
        G = 0.0;
        B = 1.0;
    }
    else if (wavelength > 440.0 && wavelength <= 490.0) {
        R = 0.0;
        G = (wavelength - 440.0) / (490.0 - 440.0);
        B = 1.0;
    }
    else if (wavelength > 490.0 && wavelength <= 510.0) {
        R = 0.0;
        G = 1.0;
        B = -1.0 * (wavelength - 510.0) / (510.0 - 490.0);
    }
    else if (wavelength > 510.0 && wavelength <= 580.0) {
        R = (wavelength - 510.0) / (580.0 - 510.0);
        G = 1.0;
        B = 0.0;
    }
    else if (wavelength > 580.0 && wavelength <= 645.0) {
        R = 1.0;
        G = -1.0 * (wavelength - 645.0) / (645.0 - 580.0);
        B = 0.0;
    }
    else if (wavelength > 645.0 && wavelength <= 750.0) {
        R = 1.0;
        G = 0.0;
        B = 0.0;
    }

    // Let the intensity fall off near the vision limits
    float factor = 0.0;
    if (wavelength >= 380.0 && wavelength <= 420.0) {
        factor = 0.3 + 0.7 * (wavelength - 380.0) / (420.0 - 380.0);
    }
    else if (wavelength > 420.0 && wavelength <= 700.0) {
        factor = 1.0;
    }
    else if (wavelength > 700.0 && wavelength <= 750.0) {
        factor = 0.3 + 0.7 * (750.0 - wavelength) / (750.0 - 700.0);
    }

    R *= factor;
    G *= factor;
    B *= factor;

    return glm::vec3(R, G, B);
}

float rgbToWavelength(float r, float g, float b) {
    float wavelength = 0.0;
    float maxComponent = std::max({ r, g, b });

    if (r >= g && r >= b) {
        // Red dominant
        wavelength = 620 + (750 - 620) * (r / maxComponent);
    }
    else if (g >= r && g >= b) {
        // Green dominant
        wavelength = 495 + (570 - 495) * (g / maxComponent);
    }
    else if (b >= r && b >= g) {
        // Blue dominant
        wavelength = 450 + (495 - 450) * (b / maxComponent);
    }

    return wavelength;
}

glm::vec3 normalizeRGB(const glm::vec3& color) {
    float sum = color.r + color.g + color.b;
    return (sum > 0.0f) ? (color / sum) : glm::vec3(0.0f);
}
