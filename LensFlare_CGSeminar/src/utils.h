#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

glm::vec3 translateToCameraSpace(const glm::vec3& cameraPos, const glm::vec3& cameraForward, const glm::vec3& cameraUp, const glm::vec3& point);
glm::vec2 getYawandPitch(const glm::vec3& cameraPos, const glm::vec3& cameraForward, const glm::vec3& cameraUp, const glm::vec3& lightPos);
glm::vec3 wavelengthToRGB(float wavelength);
float rgbToWavelength(float r, float g, float b);
glm::vec3 normalizeRGB(const glm::vec3& color);
