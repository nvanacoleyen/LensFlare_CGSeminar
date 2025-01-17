#include "quad.h"
#include <glm/gtc/type_ptr.hpp>

// https://www.khronos.org/opengl/wiki/Tutorial2:_VAOs,_VBOs,_Vertex_and_Fragment_Shaders_(C_/_SDL)

FlareQuad::FlareQuad() {
    glGenVertexArrays(1, &m_vao_quad);
    glGenBuffers(1, &m_vbo_quad);
    glGenBuffers(1, &m_ebo_quad);
    m_pointsUpdated = true; // Flag indicating points need to be uploaded
}

FlareQuad::FlareQuad(std::vector<glm::vec3> points, int id) : m_points(points), m_pointsUpdated(true) {
    glGenVertexArrays(1, &m_vao_quad);
    glGenBuffers(1, &m_vbo_quad);
    glGenBuffers(1, &m_ebo_quad);
    m_id = id;
}

void FlareQuad::setPoints(std::vector<glm::vec3> points) {
    m_points = points;
    m_pointsUpdated = true; // Mark points as updated
}

std::vector<glm::vec3> FlareQuad::getPoints() {
    return m_points;
}

void FlareQuad::releaseArrayAndBuffer() {
    glDeleteVertexArrays(1, &m_vao_quad);
    glDeleteBuffers(1, &m_vbo_quad);
    glDeleteBuffers(1, &m_ebo_quad);
}

void FlareQuad::uploadDataIfNeeded() {
    if (m_pointsUpdated) {
        glBindVertexArray(m_vao_quad);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_quad);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * m_points.size(), m_points.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo_quad);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int) * m_indices.size(), m_indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        m_pointsUpdated = false; // Reset the update flag
    }
}

void FlareQuad::drawQuad(glm::mat4 projection, glm::mat2x2 Ma, glm::mat2x2 Ms, glm::vec3 color, GLuint texApt, glm::vec2 yawandPitch, float entrancePupilHeight, glm::mat4 sensorMatrix, float irisApertureHeight) {
    // Upload data if points have changed
    uploadDataIfNeeded();

    // Bind VAO
    glBindVertexArray(m_vao_quad);

    // Set shader uniforms
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(projection)); // Projection Matrix
    glUniform3fv(1, 1, glm::value_ptr(color)); // Color
    glUniformMatrix2fv(2, 1, GL_FALSE, glm::value_ptr(Ma)); // Projection Matrix
    glUniformMatrix2fv(3, 1, GL_FALSE, glm::value_ptr(Ms)); // Projection Matrix
    glUniform1fv(4, 1, &yawandPitch.x);
    glUniform1fv(5, 1, &yawandPitch.y);
    glUniform1fv(6, 1, &entrancePupilHeight);
    glUniformMatrix4fv(8, 1, GL_FALSE, glm::value_ptr(sensorMatrix));
    glUniform1fv(9, 1, &irisApertureHeight);

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texApt);
    glUniform1i(7, 0);

    // Draw the quad
    glDrawArrays(GL_LINE_STRIP, 0, m_points.size());
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
