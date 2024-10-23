#include "lens_system.h"

#include "ray_transfer_matrices.h"

LensSystem::LensSystem(int irisAperturePos, std::vector<LensInterface> lensInterfaces) {
	m_iris_aperture_pos = irisAperturePos;
	m_lens_interfaces = lensInterfaces;
	this->generateLineDrawers();
}

void LensSystem::setIrisAperturePos(int newPos) {
	m_iris_aperture_pos = newPos;
}

int LensSystem::getIrisAperturePos() {
	return m_iris_aperture_pos;
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
		if (i == 0) {
			rayTransferMatrices.push_back(rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, 1.f, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri));
		}
		else {
			rayTransferMatrices.push_back(rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i-1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri));
		}
	}
	return rayTransferMatrices;
}

std::vector<glm::mat2x2> LensSystem::getRayTransferMatricesWithReflection(int firstReflectionPos, int secondReflectionPos) {
	std::vector<glm::mat2x2> rayTransferMatrices;
	RayTransferMatrixBuilder rayTransferMatrixBuilder;
	//check if reflection makes sense
	if (firstReflectionPos > secondReflectionPos && secondReflectionPos >= 0 && firstReflectionPos < m_lens_interfaces.size()) {
		//iterate until first reflection
		for (int i = 0; i < firstReflectionPos; i++) {
			if (i == 0) {
				rayTransferMatrices.push_back(rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, 1.f, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri));
			}
			else {
				rayTransferMatrices.push_back(rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri));
			}
		}
		rayTransferMatrices.push_back(rayTransferMatrixBuilder.getReflectionMatrix(m_lens_interfaces[firstReflectionPos].Ri)); //reflection step
		for (int i = firstReflectionPos - 1; i > secondReflectionPos; i--) {
			rayTransferMatrices.push_back(rayTransferMatrixBuilder.getinverseRefractionBackwardsTranslationMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri));
		}
		//rayTransferMatrices.push_back(glm::inverse(rayTransferMatrixBuilder.getReflectionMatrix(m_lens_interfaces[secondReflectionPos].Ri))); //reflection step
		rayTransferMatrices.push_back(rayTransferMatrixBuilder.getReflectionMatrix(-m_lens_interfaces[secondReflectionPos].Ri)); //reflection step
		for (int i = secondReflectionPos + 1; i < m_lens_interfaces.size(); i++) {
			rayTransferMatrices.push_back(rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri));
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

std::vector<float> LensSystem::getInterfacePositionsWithReflections(int firstReflectionPos, int secondReflectionPos) {
	std::vector<float> interfacePositions;
	float pos = 0.0f;
	//check if reflection makes sense
	if (firstReflectionPos > secondReflectionPos && secondReflectionPos >= 0 && firstReflectionPos < m_lens_interfaces.size()) {
		for (int i = 0; i < firstReflectionPos; i++) {
			interfacePositions.push_back(pos);
			pos += m_lens_interfaces[i].di;
		}
		//interfacePositions.push_back(pos);
		for (int i = firstReflectionPos; i > secondReflectionPos; i--) {
			interfacePositions.push_back(pos);
			pos -= m_lens_interfaces[i-1].di;
		}
		//interfacePositions.push_back(pos);
		for (int i = secondReflectionPos; i < m_lens_interfaces.size(); i++) {
			interfacePositions.push_back(pos);
			pos += m_lens_interfaces[i].di;
		}
	}
	return interfacePositions;
}

float LensSystem::getEntrancePupilHeight() {
	if (m_lens_interfaces.size() > 0) {
		return m_lens_interfaces[0].hi;
	}
	else {
		return 0.f;
	}
}

float LensSystem::getIrisApertureHeight() {
	if (m_lens_interfaces.size() > m_iris_aperture_pos) {
		return m_lens_interfaces[m_iris_aperture_pos].hi;
	}
	else {
		return 0.f;
	}
}

float LensSystem::getSensorPosition() {
	float pos = 0.0f;
	for (LensInterface lensInterface : m_lens_interfaces) {
		pos += lensInterface.di;
	}
	return pos;
}

glm::vec2 LensSystem::getCircleCenter(float z, float h, float R) {
	//https://www.mathsisfun.com/geometry/arc.html
	//https://www.mathopenref.com/sagitta.html
	bool isConvex = false;
	if (R > 0.f) {
		isConvex = true;
	}
	R = abs(R);
	float arcHeigth = R - sqrt(pow(R, 2.f) - pow(h/2.f, 2.f));
	if (isConvex) {
		return glm::vec2(z + (R - arcHeigth), 0.0f);
	}
	else {
		return glm::vec2(z - (R - arcHeigth), 0.0f);
	}
}


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
		//Flat interfaces
		if (abs(m_lens_interfaces[i].Ri) > 1000.f) {
			liPoints.push_back(glm::vec3(interfacePositions[i], m_lens_interfaces[i].hi / 2, 0.f));
			liPoints.push_back(glm::vec3(interfacePositions[i], -m_lens_interfaces[i].hi / 2, 0.f));
		}
		else {
			float R = m_lens_interfaces[i].Ri;
			float height = m_lens_interfaces[i].hi;
			glm::vec2 circleCenter = getCircleCenter(interfacePositions[i], height, R);

			//Change angle here to change resolution of curved lens interfaces
			double angleIncrement = 0.1 * 3.141593f / 180.0f;

			glm::vec3 startPoint = glm::vec3(interfacePositions[i], height / 2, 0.f);
			glm::vec3 endPoint = glm::vec3(interfacePositions[i], -height / 2, 0.f);
			if (R < 0.f) {
				startPoint = glm::vec3(interfacePositions[i], -height / 2, 0.f);
				endPoint = glm::vec3(interfacePositions[i], height / 2, 0.f);
			}
			float startAngle = std::atan2(startPoint.y - circleCenter.y, startPoint.x - circleCenter.x);
			liPoints.push_back(startPoint);

			float angle = startAngle;
			float nextX;
			if (R > 0.f) {
				do {
					angle += angleIncrement;
					float x = circleCenter.x + R * std::cos(angle);
					float y = circleCenter.y + R * std::sin(angle);
					liPoints.push_back(glm::vec3(x, y, 0.f));
					nextX = circleCenter.x + R * std::cos(angle + angleIncrement);
				} while (nextX < endPoint.x);
			}
			else {
				do {
					angle += angleIncrement;
					float x = circleCenter.x - R * std::cos(angle);
					float y = circleCenter.y - R * std::sin(angle);
					liPoints.push_back(glm::vec3(x, y, 0.f));
					nextX = circleCenter.x - R * std::cos(angle + angleIncrement);
				} while (nextX > endPoint.x);
			}
			liPoints.push_back(endPoint);
			liPoints.push_back(startPoint);

		}
		m_line_drawers.push_back(LineDrawer(liPoints));
	}
}

void LensSystem::drawLensSystem(glm::mat4 projection) {
	for (int i = 0; i < m_line_drawers.size(); i++) {
		if (i == m_iris_aperture_pos) {
			m_line_drawers[i].drawLine(projection, glm::vec3(0.5f, 1.f, 0.5f));
		}
		else {
			m_line_drawers[i].drawLine(projection, glm::vec3(0.5f, 0.5f, 1.f));
		}
		
	}
}
