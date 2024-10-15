#pragma once

#include "line_drawer.h"

#include <glm/glm.hpp>
#include <vector>

class RayPropagationDrawer {
public:
	RayPropagationDrawer(std::vector<glm::mat2x2> rayTransferMatrices, std::vector<float> interfacePositions, glm::vec2 ray);
	void setRayTransferMatrices(std::vector<glm::mat2x2> rayTransferMatrices);
	void setInterfacePositions(std::vector<float> interfacePositions);
	void setRay(glm::vec2 ray);
	void generateLineDrawers();
	void drawRayPropagation(glm::mat4 projection);
private:
	LineDrawer raytoLine(float z, glm::vec2 ray);

	std::vector<glm::mat2x2> m_ray_transfer_matrices;
	std::vector<float> m_interface_positions;
	glm::vec2 m_ray;
	std::vector<LineDrawer> m_line_drawers;
};