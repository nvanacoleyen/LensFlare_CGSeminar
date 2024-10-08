#include "line_drawer.h"

#include <glm/gtc/type_ptr.hpp>


LineDrawer::LineDrawer() {
	glGenVertexArrays(1, &m_vaoLine);
	glGenBuffers(1, &m_vboLine);
}

LineDrawer::LineDrawer(std::vector<glm::vec3> points) {
	glGenVertexArrays(1, &m_vaoLine);
	glGenBuffers(1, &m_vboLine);
	m_points = points;
}

void LineDrawer::setPoints(std::vector<glm::vec3> points) {
	m_points = points;
}

std::vector<glm::vec3> LineDrawer::getPoints() {
	return m_points;
}

void LineDrawer::drawLine(glm::mat4 projection) {

	//VERTEX ARRAYS AND BUFFERS
	glBindVertexArray(m_vaoLine);
	glBindBuffer(GL_ARRAY_BUFFER, m_vboLine);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * m_points.size(), m_points.data(), GL_STATIC_DRAW); 

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	//SHADER UNIFORMS
	glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(projection)); // Projection Matrix
	glUniform3fv(1, 1, glm::value_ptr(glm::vec3(1.f, 1.f, 1.f))); // Color

	//DRAW
	glDrawArrays(GL_LINE_STRIP, 0, m_points.size());

}