#pragma once

#include <glm/glm.hpp>
#include <vector>

struct LensInterface {
	float di; //positive displacement to the next interface at interface i (from thickness)
	float ni; //the refractive indices at interface i
	float Ri; //the lens radius (R > 0 / R = inf for convex / flat interfaces)
	float lambda0 = 550.f; //Wavelength that the coating handles
};

class LensSystem {
public:
	LensSystem(int irisAperturePos, float apertureHeight, float entrancePupilHeight, std::vector<LensInterface>& lensInterfaces);
	void setIrisAperturePos(int newPos);
	int getIrisAperturePos() const;
	void setApertureHeight(float newHeight);
	float getApertureHeight() const;
	void setEntrancePupilHeight(float newHeight);
	float getEntrancePupilHeight() const;
	std::vector<LensInterface> getLensInterfaces() const;
	void setLensInterfaces(std::vector<LensInterface> newLensInterfaces);
	std::vector<glm::mat2x2> getRayTransferMatrices();
	std::vector<glm::mat2x2> getRayTransferMatricesWithReflection(int firstReflectionPos, int secondReflectionPos);
	glm::mat2x2 getMa() const;
	glm::mat2x2 getMs();
	glm::mat2x2 getMa(int firstReflectionPos, int secondReflectionPos) const;
	glm::mat2x2 getMs(int firstReflectionPos, int secondReflectionPos);
	std::vector<glm::vec2> getPreAptReflections();
	std::vector<glm::vec2> getPostAptReflections();
	std::vector<glm::mat2x2> getMa(std::vector<glm::vec2> reflectionPos) const;
	std::vector<glm::mat2x2> getMs(std::vector<glm::vec2> reflectionPos);
	std::vector<float> getInterfacePositions();
	std::vector<float> getInterfacePositionsWithReflections(int firstReflectionPos, int secondReflectionPos);
	glm::vec3 computeFresnelAR(float theta0, float d1, float n0, float n1, float n2) const;
	glm::vec3 propagateTransmission(int firstReflectionPos, int secondReflectionPos, glm::vec2 ray) const;
	std::vector<glm::vec3> getTransmission(std::vector<glm::vec2> reflectionPos, std::vector<glm::vec2> xRays, std::vector<glm::vec2> yRays) const;
	std::vector<glm::vec3> getTransmission(std::vector<glm::vec2> reflectionPos, glm::vec2 xRay, glm::vec2 yRay) const;
	std::vector<glm::vec2> getPathIncidentAngleAtReflectionPos(glm::vec2 reflectionPair, glm::vec2 yawAndPitch);
	float m_aperture_height = 0;
	float m_entrance_pupil_height = 0;

private:
	std::vector<LensInterface> m_lens_interfaces;
	int m_iris_aperture_pos = 0;
	
};
