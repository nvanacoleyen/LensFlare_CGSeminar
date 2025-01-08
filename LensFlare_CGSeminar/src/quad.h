#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>

class FlareQuad {
public:
	FlareQuad();
	FlareQuad(std::vector<glm::vec3> points); //must be array of 4 points!
	void setPoints(std::vector<glm::vec3> points);
	std::vector<glm::vec3> getPoints();
	void releaseArrayAndBuffer();
	void drawQuad(glm::mat4 projection, glm::mat2x2 Ma, glm::mat2x2 Ms, glm::vec3 color, GLuint texApt, float light_angle_x, float light_angle_y, float entrancePupilHeight);

private:
	std::vector<glm::vec3> m_points;
	std::vector<int> m_indices = { 0, 1, 3, 1, 2, 3 };
	GLuint m_vao_quad;
	GLuint m_vbo_quad;
	GLuint m_ebo_quad;
};
