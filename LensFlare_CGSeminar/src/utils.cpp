#include "utils.h"

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
    float yaw = atan2(cameraSpacePoint.x, cameraSpacePoint.z);
    float pitch = atan2(cameraSpacePoint.y, cameraSpacePoint.z);

    //std::cout << "Yaw: " << yaw << ", Pitch: " << pitch << std::endl;

    return glm::vec2(yaw, pitch);
}
