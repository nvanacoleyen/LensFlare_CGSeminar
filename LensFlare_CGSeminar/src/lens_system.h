#pragma once

#include <glm/glm.hpp>
#include <vector>

struct LensInterface {
	float di; //positive displacement to the next interface at interface i (from thickness)
	float ni; //the refractive indices at interface i
	float Ri; //the lens radius (R > 0 / R = inf for convex / flat interfaces)
	float hi; //the lens height
};

class LensSystem {
public:
	LensSystem(int irisAperturePos, float sensorSize, std::vector<LensInterface> lensInterfaces);
	void setIrisAperturePos(int newPos);
	int getIrisAperturePos();
	void setSensorSize(float newSize);
	float getSensorSize();
	std::vector<LensInterface> getLensInterfaces();
	void setLensInterfaces(std::vector<LensInterface> newLensInterfaces);
	std::vector<glm::mat2x2> getRayTransferMatrices();
	std::vector<glm::mat2x2> getRayTransferMatricesWithReflection(int firstReflectionPos, int secondReflectionPos);
	glm::mat2x2 getMa();
	glm::mat2x2 getMs();
	glm::mat2x2 getMa(int firstReflectionPos, int secondReflectionPos);
	glm::mat2x2 getMs(int firstReflectionPos, int secondReflectionPos);
	std::vector<glm::vec2> getPreAptReflections();
	std::vector<glm::vec2> getPostAptReflections();
	std::vector<glm::mat2x2> getMa(std::vector<glm::vec2> reflectionPos);
	std::vector<glm::mat2x2> getMs(std::vector<glm::vec2> reflectionPos);
	std::vector<float> getInterfacePositions();
	std::vector<float> getInterfacePositionsWithReflections(int firstReflectionPos, int secondReflectionPos);
	float getEntrancePupilHeight();
	float getIrisApertureHeight();
	float getSensorPosition();

private:
	std::vector<LensInterface> m_lens_interfaces;
	int m_iris_aperture_pos = 0;
	float m_sensor_size = 0;
};