#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "line_drawer.h"

struct LensInterface {
	float di; //positive displacement to the next interface at interface i (from thickness)
	float ni; //the refractive indices at interface i
	float Ri; //the lens radius (R > 0 / R = inf for convex / flat interfaces)
	float hi; //the lens height
};

class LensSystem {
public:
	LensSystem(float entrancePupilHeight, float irisApertureHeight);
	LensSystem(float entrancePupilHeight, float irisApertureHeight, std::vector<LensInterface> lensInterfaces);
	std::vector<LensInterface> getLensInterfaces();
	void setLensInterfaces(std::vector<LensInterface> newLensInterfaces);
	std::vector<glm::mat2x2> getRayTransferMatrices();
	std::vector<float> getInterfacePositions();
	float getSensorPosition();
	void generateLineDrawers();
	void drawLensSystem(glm::mat4 projection);

private:
	glm::vec2 getCircleCenter(float z, float h, float R);
	std::vector<LensInterface> m_lens_interfaces;
	std::vector<LineDrawer> m_line_drawers;
	std::vector<glm::mat2x2> m_ray_transfer_matrices; //Needs to be updated if lens interfaces change
	float m_entrance_pupil_height = 10.f;
	float m_iris_aperture_height = 10.f;
};