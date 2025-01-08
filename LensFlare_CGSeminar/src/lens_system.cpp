#include "lens_system.h"

#include "ray_transfer_matrices.h"
#include <iostream>
#include <string>

LensSystem::LensSystem(int irisAperturePos, float sensorSize, std::vector<LensInterface> lensInterfaces) {
	m_iris_aperture_pos = irisAperturePos;
	m_lens_interfaces = lensInterfaces;
	m_sensor_size = sensorSize;
}

void LensSystem::setIrisAperturePos(int newPos) {
	m_iris_aperture_pos = newPos;
}

int LensSystem::getIrisAperturePos() {
	return m_iris_aperture_pos;
}

void LensSystem::setSensorSize(float newSize) {
	m_sensor_size = newSize;
}

float LensSystem::getSensorSize() {
	return m_sensor_size;
}

std::vector<LensInterface> LensSystem::getLensInterfaces() {
	return m_lens_interfaces;
}

void LensSystem::setLensInterfaces(std::vector<LensInterface> newLensInterfaces) {
	m_lens_interfaces = newLensInterfaces;
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
		rayTransferMatrices.push_back(rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[secondReflectionPos].di));
		//rayTransferMatrices.push_back(glm::inverse(rayTransferMatrixBuilder.getReflectionMatrix(m_lens_interfaces[secondReflectionPos].Ri))); //reflection step
		rayTransferMatrices.push_back(rayTransferMatrixBuilder.getReflectionMatrix(-m_lens_interfaces[secondReflectionPos].Ri)); //reflection step
		rayTransferMatrices.push_back(rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[secondReflectionPos].di));
		for (int i = secondReflectionPos + 1; i < m_lens_interfaces.size(); i++) {
			rayTransferMatrices.push_back(rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri));
		}
	}
	return rayTransferMatrices;
}

glm::mat2x2 LensSystem::getMa() {
	glm::mat2x2 Ma = glm::mat2(1.0f);
	RayTransferMatrixBuilder rayTransferMatrixBuilder;
	for (int i = 0; i < m_iris_aperture_pos; i++) {
		if (i == 0) {
			Ma = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, 1.f, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ma;
		}
		else {
			Ma = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ma;
		}
	}
	return Ma;
}

glm::mat2x2 LensSystem::getMs() {
	glm::mat2x2 Ms = glm::mat2(1.0f);
	RayTransferMatrixBuilder rayTransferMatrixBuilder;
	for (int i = m_iris_aperture_pos; i < m_lens_interfaces.size(); i++) {
		if (i == 0) {
			Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, 1.f, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ms;
		}
		else {
			Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ms;
		}
	}
	return Ms;
}

glm::mat2x2 LensSystem::getMa(int firstReflectionPos, int secondReflectionPos) {
	glm::mat2x2 Ma = glm::mat2(1.0f);
	RayTransferMatrixBuilder rayTransferMatrixBuilder;
	//check if reflection makes sense
	if (firstReflectionPos > secondReflectionPos && secondReflectionPos >= 0 && firstReflectionPos < m_lens_interfaces.size()) {
		//check if reflection happens before aperture
		if (firstReflectionPos < m_iris_aperture_pos && secondReflectionPos < m_iris_aperture_pos) {
			for (int i = 0; i < firstReflectionPos; i++) {
				if (i == 0) {
					Ma = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, 1.f, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ma;
				}
				else {
					Ma = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ma;
				}
			}
			Ma = rayTransferMatrixBuilder.getReflectionMatrix(m_lens_interfaces[firstReflectionPos].Ri) * Ma; //reflection step
			for (int i = firstReflectionPos - 1; i > secondReflectionPos; i--) {
				Ma = rayTransferMatrixBuilder.getinverseRefractionBackwardsTranslationMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ma;
			}
			Ma = rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[secondReflectionPos].di) * Ma;
			Ma = rayTransferMatrixBuilder.getReflectionMatrix(-m_lens_interfaces[secondReflectionPos].Ri) * Ma; //reflection step
			Ma = rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[secondReflectionPos].di) * Ma;
			for (int i = secondReflectionPos + 1; i < m_iris_aperture_pos; i++) {
				Ma = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ma;
			}
		}
		else {
			for (int i = 0; i < m_iris_aperture_pos; i++) {
				if (i == 0) {
					Ma = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, 1.f, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ma;
				}
				else {
					Ma = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ma;
				}
			}
		}
	}

	return Ma;
}

glm::mat2x2 LensSystem::getMs(int firstReflectionPos, int secondReflectionPos) {
	glm::mat2x2 Ms = glm::mat2(1.0f);
	RayTransferMatrixBuilder rayTransferMatrixBuilder;
	//check if reflection makes sense
	if (firstReflectionPos > secondReflectionPos && secondReflectionPos >= 0 && firstReflectionPos < m_lens_interfaces.size()) {
		//check if reflection happens after aperture
		if (firstReflectionPos > m_iris_aperture_pos && secondReflectionPos > m_iris_aperture_pos) {
			for (int i = m_iris_aperture_pos; i < firstReflectionPos; i++) {
				if (i == 0) {
					Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, 1.f, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ms;
				}
				else {
					Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ms;
				}
			}
			Ms = rayTransferMatrixBuilder.getReflectionMatrix(m_lens_interfaces[firstReflectionPos].Ri) * Ms; //reflection step
			for (int i = firstReflectionPos - 1; i > secondReflectionPos; i--) {
				Ms = rayTransferMatrixBuilder.getinverseRefractionBackwardsTranslationMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ms;
			}
			Ms = rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[secondReflectionPos].di) * Ms;
			Ms = rayTransferMatrixBuilder.getReflectionMatrix(-m_lens_interfaces[secondReflectionPos].Ri) * Ms; //reflection step
			Ms = rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[secondReflectionPos].di) * Ms;
			for (int i = secondReflectionPos + 1; i < m_lens_interfaces.size(); i++) {
				Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ms;
			}
		}
		else {
			for (int i = m_iris_aperture_pos; i < m_lens_interfaces.size(); i++) {
				if (i == 0) {
					Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, 1.f, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ms;
				}
				else {
					Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * Ms;
				}
			}
		}
	}

	return Ms;
}

std::vector<glm::vec2> LensSystem::getPreAptReflections() {
	std::vector<glm::vec2> reflectionPairs;
	for (int i = 1; i < m_iris_aperture_pos; i++) {
		for (int j = i - 1; j >= 0; j--) {
			reflectionPairs.push_back({ i, j });
		}
	}
	return reflectionPairs;
}

std::vector<glm::vec2> LensSystem::getPostAptReflections() {
	std::vector<glm::vec2> reflectionPairs;
	for (int i = m_iris_aperture_pos + 2; i < m_lens_interfaces.size(); i++) {
		for (int j = i - 1; j > m_iris_aperture_pos; j--) {
			reflectionPairs.push_back({ i, j });
		}
	}
	return reflectionPairs;
}

std::vector<glm::mat2x2> LensSystem::getMa(std::vector<glm::vec2> reflectionPos) {
	std::vector<glm::mat2x2> Mas;
	for (glm::vec2 reflectionPair : reflectionPos) {
		Mas.push_back(this->getMa(reflectionPair.x, reflectionPair.y));
	}
	return Mas;
}
std::vector<glm::mat2x2> LensSystem::getMs(std::vector<glm::vec2> reflectionPos) {
	std::vector<glm::mat2x2> Mss;
	for (glm::vec2 reflectionPair : reflectionPos) {
		Mss.push_back(this->getMs(reflectionPair.x, reflectionPair.y));
	}
	return Mss;
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
		interfacePositions.push_back(pos);
		for (int i = firstReflectionPos; i > secondReflectionPos; i--) {
			interfacePositions.push_back(pos);
			pos -= m_lens_interfaces[i-1].di;
		}
		interfacePositions.push_back(pos);
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

