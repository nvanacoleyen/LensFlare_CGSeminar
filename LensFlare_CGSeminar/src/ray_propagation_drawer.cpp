#include "ray_propagation_drawer.h"

RayPropagationDrawer::RayPropagationDrawer(std::vector<glm::mat2x2> rayTransferMatrices, std::vector<float> interfacePositions, glm::vec2 ray) {
	m_ray_transfer_matrices = rayTransferMatrices;
	m_interface_positions = interfacePositions;
	m_ray = ray;
	this->generateLineDrawers();
}

void RayPropagationDrawer::setRayTransferMatrices(std::vector<glm::mat2x2> rayTransferMatrices) {
	m_ray_transfer_matrices = rayTransferMatrices;
	this->generateLineDrawers();
}

void RayPropagationDrawer::setInterfacePositions(std::vector<float> interfacePositions) {
	m_interface_positions = interfacePositions;
	this->generateLineDrawers();
}

void RayPropagationDrawer::setRay(glm::vec2 ray) {
	m_ray = ray;
	this->generateLineDrawers();
}

void RayPropagationDrawer::generateLineDrawers() {
	if (m_line_drawers.size() > 0) {
		for (LineDrawer lineDrawer : m_line_drawers) {
			lineDrawer.releaseArrayAndBuffer();
		}
	}
	m_line_drawers.clear();
	//m_line_drawers.push_back(raytoLine(-2, m_ray));
	glm::vec2 transformedRay = m_ray;
	for (int i = 0; i < m_ray_transfer_matrices.size(); i++) {
		//for now assuming interface size is same as matrices size (so no reflection)
		m_line_drawers.push_back(raytoLine(m_interface_positions[i], transformedRay));
		transformedRay = m_ray_transfer_matrices[i] * transformedRay;
		
	}
	m_line_drawers.push_back(raytoLine(m_interface_positions[m_ray_transfer_matrices.size()-1]+2.f, transformedRay));
}

void RayPropagationDrawer::drawRayPropagation(glm::mat4 projection) {
	for (LineDrawer lineDrawer : m_line_drawers) {
		lineDrawer.drawLine(projection);
	}
}

LineDrawer RayPropagationDrawer::raytoLine(float z, glm::vec2 ray) {
	std::vector<glm::vec3> rayLine;
	//rayLine.push_back(glm::vec3(z, ray.x, 0.0f));
	//rayLine.push_back(glm::vec3(z + cos(ray.y), ray.x + sin(ray.y), 0.0f));
	rayLine.push_back(glm::vec3(z - cos(ray.y), ray.x - sin(ray.y), 0.0f));
	rayLine.push_back(glm::vec3(z, ray.x, 0.0f));
	return LineDrawer(rayLine);
}