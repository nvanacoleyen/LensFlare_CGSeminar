#include "lens_system.h"

#include "ray_transfer_matrices.h"

LensSystem::LensSystem(float entrancePupilHeight, float irisApertureHeight) {
	m_entrance_pupil_height = entrancePupilHeight;
	m_iris_aperture_height = irisApertureHeight;
}

LensSystem::LensSystem(float entrancePupilHeight, float irisApertureHeight, std::vector<LensInterface> lensInterfaces) {
	m_entrance_pupil_height = entrancePupilHeight;
	m_iris_aperture_height = irisApertureHeight;
	m_lens_interfaces = lensInterfaces;
	this->generateLineDrawers();
}

std::vector<LensInterface> LensSystem::getLensInterfaces() {
	return m_lens_interfaces;
}

void LensSystem::setLensInterfaces(std::vector<LensInterface> newLensInterfaces) {
	m_lens_interfaces = newLensInterfaces;
	this->generateLineDrawers();
}

std::vector<glm::mat2x2> LensSystem::getRayTransferMatrices() {
	std::vector<glm::mat2x2> rayTransferMatrices;
	RayTransferMatrixBuilder rayTransferMatrixBuilder;
	for (int i = 0; i < m_lens_interfaces.size(); i++) {
		if (i == m_lens_interfaces.size() - 1) {
			rayTransferMatrices.push_back(rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[i].di));
		}
		else {
			rayTransferMatrices.push_back(rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i].ni, m_lens_interfaces[i+1].ni, m_lens_interfaces[i].Ri));
		}
	}
	return rayTransferMatrices;
}

std::vector<float> LensSystem::getInterfacePositions() {
	std::vector<float> interfacePositions;
	float pos = 0.0f;
	for (LensInterface lensInterface : m_lens_interfaces) {
		interfacePositions.push_back(pos);
		pos += lensInterface.di;
	}
	return interfacePositions;
}

//Needs to be updated to draw the spherical interfaces
//https://www.mathsisfun.com/geometry/arc.html
void LensSystem::generateLineDrawers() {
	if (m_line_drawers.size() > 0) {
		for (LineDrawer lineDrawer : m_line_drawers) {
			lineDrawer.releaseArrayAndBuffer();
		}
	}
	m_line_drawers.clear();
	std::vector<float> interfacePositions = this->getInterfacePositions();
	for (int i = 0; i < m_lens_interfaces.size(); i++) {
		std::vector<glm::vec3> liPoints;
		if (abs(m_lens_interfaces[i].Ri) > 1000.f) {
			liPoints.push_back(glm::vec3(interfacePositions[i], m_entrance_pupil_height / 2, 0.f));
			liPoints.push_back(glm::vec3(interfacePositions[i], -m_entrance_pupil_height / 2, 0.f));
		}
		else {
			int numPoints = 20;
			double thetaStart, thetaEnd;
			double centerX = interfacePositions[i];
			double centerY = 0.0;
			float R = m_lens_interfaces[i].Ri;
			float height = m_lens_interfaces[i].hi;
			// Convex or Concave interface
			if (R > 0) {
				thetaStart = -std::asin(height / (2 * R));
				thetaEnd = std::asin(height / (2 * R));
			}
			else {
				R = -R;
				thetaStart = std::asin(height / (2 * R));
				thetaEnd = -std::asin(height / (2 * R));
			}

			for (int j = 0; j < numPoints; ++j) {
				double theta = thetaStart + j * (thetaEnd - thetaStart) / (numPoints - 1);
				double x = centerX + R * (1 - std::cos(theta));
				double y = centerY + R * std::sin(theta);
				if (m_lens_interfaces[i].Ri < 0) {
					x = centerX - R * (1 - std::cos(theta));
					y = centerY - R * std::sin(theta);
				}
				liPoints.push_back(glm::vec3(x, y, 0.0f));
			}

		}
		m_line_drawers.push_back(LineDrawer(liPoints));
	}
}

void LensSystem::drawLensSystem(glm::mat4 projection) {
	for (LineDrawer lineDrawer : m_line_drawers) {
		lineDrawer.drawLine(projection);
	}
}
