#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>

struct QuadData {
    int quadID;
    float intensityVal;
};

//check byte alignment issues if any
struct AnnotationData {
    int quadID;
    float quadHeight;
    float aptHeight;
    int padding;
    glm::vec2 quadCenterPos;
    glm::vec2 aptCenterPos;
    glm::vec4 quadColor;
};

class FlareQuad {
public:
    FlareQuad();
    FlareQuad(std::vector<glm::vec3> points, int id); // Must be an array of 4 points!
    void setPoints(std::vector<glm::vec3> points);
    std::vector<glm::vec3> getPoints();
    void releaseArrayAndBuffer();
    void drawQuad(glm::mat2x2 Ma, glm::mat2x2 Ms, glm::vec3 color);

private:
    void uploadDataIfNeeded(); // New method for data upload optimization

    std::vector<glm::vec3> m_points;
    int m_id;
    std::vector<int> m_indices = { 0, 1, 3, 1, 2, 3 };
    GLuint m_vao_quad;
    GLuint m_vbo_quad;
    GLuint m_ebo_quad;
    bool m_pointsUpdated; // Flag to track if points have been updated
};
