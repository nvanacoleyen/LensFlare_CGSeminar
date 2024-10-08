#pragma once

#include <glm/glm.hpp>
#include <vector>

struct LensInterface {
	float di; //positive displacement to the next interface at interface i (from thickness)
	float ni; //the refractive indices at interface i
	float Ri; //the lens radius (R > 0 / R = inf for convex / flat interfaces)
};

class LensSystem {
public:
	LensSystem(float entrancePupilHeight, float irisApertureHeight);
	std::vector<LensInterface> getLensInterfaces();
	void setLensInterfaces(std::vector<LensInterface> newLensInterfaces);
	void drawLensSystem(glm::mat4 projection);

private:
	std::vector<LensInterface> m_lens_interfaces;
	std::vector<glm::mat2x2> m_ray_transfer_matrices; //Needs to be updated if lens interfaces change
	float m_entrance_pupil_height;
	float m_iris_aperture_height;
};