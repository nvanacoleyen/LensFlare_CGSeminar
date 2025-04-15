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

void FlareQuad::drawQuad(glm::mat2x2& Ma, glm::mat2x2& Ms, glm::vec3& color, AnnotationData& annotationData) {
    // Upload data if points have changed
    uploadDataIfNeeded();

    // Bind VAO
    glBindVertexArray(m_vao_quad);

    // Set shader uniforms
    glUniform3fv(1, 1, glm::value_ptr(color)); // Color
    glUniformMatrix2fv(2, 1, GL_FALSE, glm::value_ptr(Ma)); // Projection Matrix
    glUniformMatrix2fv(3, 1, GL_FALSE, glm::value_ptr(Ms)); // Projection Matrix
    glUniform1i(12, m_id); // Quad ID
    glUniform2fv(15, 1, glm::value_ptr(annotationData.posAnnotationTransform));
    glUniform1f(16, annotationData.sizeAnnotationTransform);


    // Draw the quad
    glDrawArrays(GL_LINE_STRIP, 0, m_points.size());
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void FlareQuad::drawQuad(glm::vec3& color, AnnotationData& annotationData) {
    // Upload data if points have changed
    uploadDataIfNeeded();

    // Bind VAO
    glBindVertexArray(m_vao_quad);

    // Set shader uniforms
    glUniform2fv(2, 1, glm::value_ptr(annotationData.posAnnotationTransform));
    glUniform1f(3, annotationData.sizeAnnotationTransform);
    glUniform3fv(4, 1, glm::value_ptr(color)); // Color
    glUniform1i(8, m_id); // Quad ID

    // Draw the quad
    glDrawArrays(GL_LINE_STRIP, 0, m_points.size());
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
