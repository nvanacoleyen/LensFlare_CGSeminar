#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>

class LineDrawer {
public:
	LineDrawer();
	LineDrawer(std::vector<glm::vec3> points);
	void setPoints(std::vector<glm::vec3> points);
	std::vector<glm::vec3> getPoints();
	void drawLine(glm::mat4 projection);

private:
	std::vector<glm::vec3> m_points;
	GLuint m_vaoLine;
	GLuint m_vboLine;
};