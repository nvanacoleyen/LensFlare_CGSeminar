#include "lens_system.h"

#include "line_drawer.h"

LensSystem::LensSystem(float entrancePupilHeight, float irisApertureHeight) {
	m_entrance_pupil_height = entrancePupilHeight;
	m_iris_aperture_height = irisApertureHeight;
}

LensSystem::LensSystem(float entrancePupilHeight, float irisApertureHeight, std::vector<LensInterface> lensInterfaces) {
	m_entrance_pupil_height = entrancePupilHeight;
	m_iris_aperture_height = irisApertureHeight;
	m_lens_interfaces = lensInterfaces;
}

std::vector<LensInterface> LensSystem::getLensInterfaces() {
	return m_lens_interfaces;
}

void LensSystem::setLensInterfaces(std::vector<LensInterface> newLensInterfaces) {
	m_lens_interfaces = newLensInterfaces;
}

void LensSystem::drawLensSystem(glm::mat4 projection) {
	for (LensInterface lensInterface : m_lens_interfaces) {
		std::vector<glm::vec3> liPoints;
		liPoints.push_back(glm::vec3(lensInterface.zi, 1.f, 0.f));
		liPoints.push_back(glm::vec3(lensInterface.zi, -1.f, 0.f));
		LineDrawer(liPoints).drawLine(projection);
	}
}
