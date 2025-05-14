#include "lens_system.h"

#include "ray_transfer_matrices.h"
#include <iostream>
#include <string>
#include <numbers>
#include <algorithm> 

float RED_WAVELENGTH = 650;
float GREEN_WAVELENGTH = 550;
float BLUE_WAVELENGTH = 475;

LensSystem::LensSystem(int irisAperturePos, float apertureHeight, float entrancePupilHeight, std::vector<LensInterface>& lensInterfaces) {
	m_iris_aperture_pos = irisAperturePos;
	m_aperture_height = apertureHeight;
	m_entrance_pupil_height = entrancePupilHeight;
	m_lens_interfaces = lensInterfaces;
}

void LensSystem::setIrisAperturePos(int newPos) {
	m_iris_aperture_pos = newPos;
}

int LensSystem::getIrisAperturePos() const {
	return m_iris_aperture_pos;
}

void LensSystem::setApertureHeight(float newHeight) {
	m_aperture_height = newHeight;
}

float LensSystem::getApertureHeight() const {
	return m_aperture_height;
}

void LensSystem::setEntrancePupilHeight(float newHeight) {
	m_entrance_pupil_height = newHeight;
}

float LensSystem::getEntrancePupilHeight() const {
	return m_entrance_pupil_height;
}

std::vector<LensInterface> LensSystem::getLensInterfaces() const {
	return m_lens_interfaces;
}

void LensSystem::setLensInterfaces(std::vector<LensInterface> newLensInterfaces) {
	m_lens_interfaces = newLensInterfaces;
}

std::vector<glm::mat2x2> LensSystem::getRayTransferMatrices() {
	std::vector<glm::mat2x2> rayTransferMatrices;
	RayTransferMatrixBuilder rayTransferMatrixBuilder;

	auto effective_ni = [this](int idx) -> float {
		if (idx == m_iris_aperture_pos)
			return 1.0f;
		return m_lens_interfaces[idx].ni;
		};

	auto effective_Ri = [this](int idx) -> float {
		if (idx == m_iris_aperture_pos)
			return std::numeric_limits<float>::infinity();
		return m_lens_interfaces[idx].Ri;
		};

	for (int i = 0; i < m_lens_interfaces.size(); i++) {
		if (i == 0) {
			rayTransferMatrices.push_back(
				rayTransferMatrixBuilder.getTranslationRefractionMatrix(
					m_lens_interfaces[i].di,
					1.0f,
					effective_ni(i),
					effective_Ri(i)
				)
			);
		}
		else {
			rayTransferMatrices.push_back(
				rayTransferMatrixBuilder.getTranslationRefractionMatrix(
					m_lens_interfaces[i].di,
					effective_ni(i - 1),
					effective_ni(i),
					effective_Ri(i)
				)
			);
		}
	}
	return rayTransferMatrices;
}

std::vector<glm::mat2x2> LensSystem::getRayTransferMatricesWithReflection(int firstReflectionPos, int secondReflectionPos) {
	std::vector<glm::mat2x2> rayTransferMatrices;
	RayTransferMatrixBuilder rayTransferMatrixBuilder;

	// Check if reflection positions make sense:
	// firstReflectionPos > secondReflectionPos, secondReflectionPos >= 0, and firstReflectionPos within valid indices.
	if (firstReflectionPos > secondReflectionPos && secondReflectionPos >= 0 && firstReflectionPos < m_lens_interfaces.size()) {

		// Since the iris aperture cannot be between the reflection positions,
		// assert that it lies either at or before secondReflectionPos or at or after firstReflectionPos.
		assert(m_iris_aperture_pos <= secondReflectionPos || m_iris_aperture_pos >= firstReflectionPos);

		// Helper lambdas: if the interface index equals m_iris_aperture_pos, override its parameters.
		auto effective_ni = [this](int idx) -> float {
			if (idx == m_iris_aperture_pos)
				return 1.0f;
			return m_lens_interfaces[idx].ni;
			};

		auto effective_Ri = [this](int idx) -> float {
			if (idx == m_iris_aperture_pos)
				return std::numeric_limits<float>::infinity();
			return m_lens_interfaces[idx].Ri;
			};

		// --- Segment A: Forward propagation until the first reflection ---
		for (int i = 0; i < firstReflectionPos; i++) {
			if (i == 0) {
				rayTransferMatrices.push_back(
					rayTransferMatrixBuilder.getTranslationRefractionMatrix(
						m_lens_interfaces[i].di,
						1.f,
						effective_ni(i),
						effective_Ri(i)
					)
				);
			}
			else {
				rayTransferMatrices.push_back(
					rayTransferMatrixBuilder.getTranslationRefractionMatrix(
						m_lens_interfaces[i].di,
						effective_ni(i - 1),
						effective_ni(i),
						effective_Ri(i)
					)
				);
			}
		}

		// --- Reflection at firstReflectionPos ---
		rayTransferMatrices.push_back(
			rayTransferMatrixBuilder.getReflectionMatrix(effective_Ri(firstReflectionPos))
		);

		// --- Segment B: Backward propagation until the second reflection ---
		for (int i = firstReflectionPos - 1; i > secondReflectionPos; i--) {
			rayTransferMatrices.push_back(
				rayTransferMatrixBuilder.getinverseRefractionBackwardsTranslationMatrix(
					m_lens_interfaces[i].di,
					effective_ni(i - 1),
					effective_ni(i),
					effective_Ri(i)
				)
			);
		}

		// --- Second reflection handling: translation, reflection, and translation ---
		rayTransferMatrices.push_back(
			rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[secondReflectionPos].di)
		);
		rayTransferMatrices.push_back(
			rayTransferMatrixBuilder.getReflectionMatrix(-effective_Ri(secondReflectionPos))
		);
		rayTransferMatrices.push_back(
			rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[secondReflectionPos].di)
		);

		// --- Segment C: Forward propagation after the second reflection ---
		for (int i = secondReflectionPos + 1; i < m_lens_interfaces.size(); i++) {
			rayTransferMatrices.push_back(
				rayTransferMatrixBuilder.getTranslationRefractionMatrix(
					m_lens_interfaces[i].di,
					effective_ni(i - 1),
					effective_ni(i),
					effective_Ri(i)
				)
			);
		}
	}
	return rayTransferMatrices;
}


glm::mat2x2 LensSystem::getMa() const {
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

	// Helper lambdas that return forced parameters when at the iris aperture index
	auto effective_ni = [this](int idx) -> float {
		if (idx == m_iris_aperture_pos)
			return 1.0f;
		return m_lens_interfaces[idx].ni;
		};

	auto effective_Ri = [this](int idx) -> float {
		if (idx == m_iris_aperture_pos)
			return std::numeric_limits<float>::infinity();
		return m_lens_interfaces[idx].Ri;
		};

	// Loop over propagation starting from the iris aperture position.
	for (int i = m_iris_aperture_pos; i < m_lens_interfaces.size(); i++) {
		if (i == m_iris_aperture_pos) {
			// For the first propagation step, the "incoming" medium is 1.0f (or effective_ni(i-1) if m_iris_aperture_pos == 0)
			float previousNi = (i == 0) ? 1.0f : effective_ni(i - 1);
			Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(
				m_lens_interfaces[i].di,
				previousNi,
				effective_ni(i),
				effective_Ri(i)
			) * Ms;
		}
		else {
			Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(
				m_lens_interfaces[i].di,
				effective_ni(i - 1),
				effective_ni(i),
				effective_Ri(i)
			) * Ms;
		}
	}
	return Ms;
}


glm::mat2x2 LensSystem::getMa(int firstReflectionPos, int secondReflectionPos) const {
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

	// Check if reflection positions make sense:
	if (firstReflectionPos > secondReflectionPos &&
		secondReflectionPos >= 0 &&
		firstReflectionPos < m_lens_interfaces.size()) {

		// Helper lambdas: If the current interface index equals m_iris_aperture_pos, force a refractive index 
		// of 1.0f and a curvature of infinity.
		auto effective_ni = [this](int idx) -> float {
			return (idx == m_iris_aperture_pos) ? 1.0f : m_lens_interfaces[idx].ni;
			};
		auto effective_Ri = [this](int idx) -> float {
			return (idx == m_iris_aperture_pos) ? std::numeric_limits<float>::infinity() : m_lens_interfaces[idx].Ri;
			};

		// If both reflections happen after the iris aperture...
		if (firstReflectionPos > m_iris_aperture_pos && secondReflectionPos > m_iris_aperture_pos) {
			// Forward propagation from the iris aperture (m_iris_aperture_pos) until first reflection.
			for (int i = m_iris_aperture_pos; i < firstReflectionPos; i++) {
				if (i == m_iris_aperture_pos) {
					Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(
						m_lens_interfaces[i].di,
						1.f,
						effective_ni(i),
						effective_Ri(i)
					) * Ms;
				}
				else {
					Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(
						m_lens_interfaces[i].di,
						effective_ni(i - 1),
						effective_ni(i),
						effective_Ri(i)
					) * Ms;
				}
			}

			// Reflection at firstReflectionPos.
			Ms = rayTransferMatrixBuilder.getReflectionMatrix(effective_Ri(firstReflectionPos)) * Ms;

			// Backward propagation until the second reflection.
			for (int i = firstReflectionPos - 1; i > secondReflectionPos; i--) {
				Ms = rayTransferMatrixBuilder.getinverseRefractionBackwardsTranslationMatrix(
					m_lens_interfaces[i].di,
					effective_ni(i - 1),
					effective_ni(i),
					effective_Ri(i)
				) * Ms;
			}

			// Second reflection handling:
			Ms = rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[secondReflectionPos].di) * Ms;
			Ms = rayTransferMatrixBuilder.getReflectionMatrix(-effective_Ri(secondReflectionPos)) * Ms;
			Ms = rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[secondReflectionPos].di) * Ms;

			// Forward propagation after the second reflection.
			for (int i = secondReflectionPos + 1; i < m_lens_interfaces.size(); i++) {
				Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(
					m_lens_interfaces[i].di,
					effective_ni(i - 1),
					effective_ni(i),
					effective_Ri(i)
				) * Ms;
			}
		}
		// Otherwise, reflection does not occur after the iris aperture.
		else {
			// Propagate from the iris aperture through to the last interface.
			for (int i = m_iris_aperture_pos; i < m_lens_interfaces.size(); i++) {
				if (i == m_iris_aperture_pos) {
					Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(
						m_lens_interfaces[i].di,
						1.f,
						effective_ni(i),
						effective_Ri(i)
					) * Ms;
				}
				else {
					Ms = rayTransferMatrixBuilder.getTranslationRefractionMatrix(
						m_lens_interfaces[i].di,
						effective_ni(i - 1),
						effective_ni(i),
						effective_Ri(i)
					) * Ms;
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

			if (m_lens_interfaces[i].ni > 1.1 || m_lens_interfaces[i - 1].ni > 1.1) {
				if (j > 0 && ((m_lens_interfaces[j].ni > 1.1 || m_lens_interfaces[j - 1].ni > 1.1))) {
					reflectionPairs.push_back({ i, j });
				}
				else if (m_lens_interfaces[j].ni > 1.1) {
					reflectionPairs.push_back({ i, j });
				}
				
			}
		}
	}
	return reflectionPairs;
}

std::vector<glm::vec2> LensSystem::getPostAptReflections() {
	std::vector<glm::vec2> reflectionPairs;
	for (int i = m_iris_aperture_pos + 2; i < m_lens_interfaces.size(); i++) {
		for (int j = i - 1; j > m_iris_aperture_pos; j--) {

			if (m_lens_interfaces[i].ni > 1.1 || m_lens_interfaces[i - 1].ni > 1.1) {
				if (j > 0 && ((m_lens_interfaces[j].ni > 1.1 || m_lens_interfaces[j - 1].ni > 1.1))) {
					reflectionPairs.push_back({ i, j });
				}
				else if (m_lens_interfaces[j].ni > 1.1) {
					reflectionPairs.push_back({ i, j });
				}

			}
		}
	}
	return reflectionPairs;
}

std::vector<glm::mat2x2> LensSystem::getMa(std::vector<glm::vec2> reflectionPos) const {
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
	for (LensInterface &lensInterface : m_lens_interfaces) {
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

//compute per lens interface, adapted code from "Supplemental Material — Physically-Based Real-Time Lens Flare Rendering"
glm::vec3 LensSystem::computeFresnelAR (
	float theta0,	// angle of incidence
	float d1,		// thickness of AR coating
	float n0,		// RI of 1st medium
	float n1,		// RI of coating layer
	float n2		// RI of the 2nd medium
) const {
	// No coating between two air layers
	//if (n0 < 1.1f && n2 < 1.1f) {
	//	return glm::vec3(0.f);
	//}

	// refraction angles in coating and the 2nd medium
	float theta1 = asin(std::clamp(sin(theta0) * n0 / n1, -1.f, 1.f));
	float theta2 = asin(std::clamp(sin(theta0) * n0 / n2, -1.f, 1.f));
	// amplitude for outer refl. / transmission on topmost interface
	float rs01 = -sin(theta0-theta1) / sin(theta0 + theta1);
	float rp01 = tan(theta0-theta1) / tan(theta0 + theta1);
	float ts01 = 2 * sin(theta1) * cos(theta0) / sin(theta0 + theta1);
	float tp01 = ts01 * cos(theta0-theta1);
	// amplitude for inner reflection
	float rs12 = -sin(theta1-theta2) / sin(theta1 + theta2);
	float rp12 = tan(theta1-theta2) / tan(theta1 + theta2);
	// after passing through first surface twice :
	// 2 transmissions and 1 reflection
	float ris = ts01 * ts01 * rs12;
	float rip = tp01 * tp01 * rp12;
	// phase difference between outer and inner reflections
	float dy = d1 * n1;
	float dx = tan(theta1) * dy;
	float delay = sqrt(dx * dx + dy * dy);
	/* RED */
	float r_relPhase = 4 * std::numbers::pi / RED_WAVELENGTH * (delay - dx * sin(theta0));
	// Add up sines of different phase and amplitude
	float r_out_s2 = rs01 * rs01 + ris * ris +
		2 * rs01 * ris * cos(r_relPhase);
	float r_out_p2 = rp01 * rp01 + rip * rip +
		2 * rp01 * rip * cos(r_relPhase);
	float r_res = std::min((r_out_s2 + r_out_p2) / 2, 1.f); // reflectivity
	/* GREEN */
	float g_relPhase = 4 * std::numbers::pi / GREEN_WAVELENGTH * (delay - dx * sin(theta0));
	// Add up sines of different phase and amplitude
	float g_out_s2 = rs01 * rs01 + ris * ris +
		2 * rs01 * ris * cos(g_relPhase);
	float g_out_p2 = rp01 * rp01 + rip * rip +
		2 * rp01 * rip * cos(g_relPhase);
	float g_res = std::min((g_out_s2 + g_out_p2) / 2, 1.f); // reflectivity
	/* BLUE */
	float b_relPhase = 4 * std::numbers::pi / BLUE_WAVELENGTH * (delay - dx * sin(theta0));
	// Add up sines of different phase and amplitude
	float b_out_s2 = rs01 * rs01 + ris * ris +
		2 * rs01 * ris * cos(b_relPhase);
	float b_out_p2 = rp01 * rp01 + rip * rip +
		2 * rp01 * rip * cos(b_relPhase);
	float b_res = std::min((b_out_s2 + b_out_p2) / 2, 1.f); // reflectivity

	return  glm::vec3(r_res, g_res, b_res);
}

glm::vec3 LensSystem::propagateTransmission(int firstReflectionPos, int secondReflectionPos, glm::vec2 ray, bool quarterWaveCoating) const {
	glm::vec3 transmissions(1.f);
	RayTransferMatrixBuilder rayTransferMatrixBuilder;
	glm::vec2 propagated_ray = ray;

	auto effective_ni = [this](int idx) -> float {
		if (idx == m_iris_aperture_pos)
			return 1.0f;
		return m_lens_interfaces[idx].ni;
		};

	auto effective_Ri = [this](int idx) -> float {
		if (idx == m_iris_aperture_pos)
			return std::numeric_limits<float>::infinity();
		return m_lens_interfaces[idx].Ri;
		};

	// Modify computeOpticalParams to use effective_ni when appropriate.
	auto computeOpticalParams = [&](int i) -> std::pair<float, float> {
		float n;
		if (i == m_iris_aperture_pos) {
			n = 1.0f;
		}
		else {
			n = quarterWaveCoating
				? std::max(sqrt((i == 0 ? 1.0f : effective_ni(i - 1)) * effective_ni(i)), 1.38f)
				: m_lens_interfaces[i].c_ni;
		}
		float d = quarterWaveCoating
			? m_lens_interfaces[i].lambda0 / (4 * n)
			: m_lens_interfaces[i].c_di;
		return { n, d };
		};

	// Forward propagation until first reflection
	for (int i = 0; i < firstReflectionPos; ++i) {
		auto [n1, d1] = computeOpticalParams(i);
		transmissions *= glm::vec3(1.f) - computeFresnelAR(propagated_ray.y, d1, (i == 0 ? 1.f : effective_ni(i - 1)), n1, effective_ni(i));
		propagated_ray = rayTransferMatrixBuilder.getTranslationRefractionMatrix(
			m_lens_interfaces[i].di,
			(i == 0 ? 1.f : effective_ni(i - 1)),
			effective_ni(i),
			effective_Ri(i)) * propagated_ray;
	}

	// First reflection
	{
		auto [n1, d1] = computeOpticalParams(firstReflectionPos);
		transmissions *= computeFresnelAR(propagated_ray.y, d1, effective_ni(firstReflectionPos - 1), n1, effective_ni(firstReflectionPos));
		propagated_ray = rayTransferMatrixBuilder.getReflectionMatrix(effective_Ri(firstReflectionPos)) * propagated_ray;
	}

	// Backward propagation until second reflection
	for (int i = firstReflectionPos - 1; i > secondReflectionPos; --i) {
		auto [n1, d1] = computeOpticalParams(i);
		transmissions *= glm::vec3(1.f) - computeFresnelAR(propagated_ray.y, d1, effective_ni(i), n1, effective_ni(i - 1));
		propagated_ray = rayTransferMatrixBuilder.getinverseRefractionBackwardsTranslationMatrix(
			m_lens_interfaces[i].di,
			effective_ni(i - 1),
			effective_ni(i),
			effective_Ri(i)) * propagated_ray;
	}

	// Second reflection handling
	{
		auto [n1_second, d1_second] = computeOpticalParams(secondReflectionPos);
		transmissions *= computeFresnelAR(
			propagated_ray.y,
			d1_second,
			effective_ni(secondReflectionPos),
			n1_second,
			(secondReflectionPos == 0 ? 1.f : effective_ni(secondReflectionPos - 1))
		);

		propagated_ray = rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[secondReflectionPos].di) * propagated_ray;
		propagated_ray = rayTransferMatrixBuilder.getReflectionMatrix(-effective_Ri(secondReflectionPos)) * propagated_ray;
		propagated_ray = rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[secondReflectionPos].di) * propagated_ray;
	}

	// Forward propagation after second reflection
	for (int i = secondReflectionPos + 1; i < m_lens_interfaces.size(); ++i) {
		auto [n1, d1] = computeOpticalParams(i);
		transmissions *= glm::vec3(1.f) - computeFresnelAR(propagated_ray.y, d1, effective_ni(i - 1), n1, effective_ni(i));
		propagated_ray = rayTransferMatrixBuilder.getTranslationRefractionMatrix(
			m_lens_interfaces[i].di,
			effective_ni(i - 1),
			effective_ni(i),
			effective_Ri(i)) * propagated_ray;
	}

	return transmissions;
}



//Per ghost trace ray through system to get reflectance/transmission of color
std::vector<glm::vec3> LensSystem::getTransmission(std::vector<glm::vec2> reflectionPos, std::vector<glm::vec2> xRays, std::vector<glm::vec2> yRays, bool quarterWaveCoating) const {
	std::vector<glm::vec3> results;
	for (int i = 0; i < reflectionPos.size(); i++) {
		results.push_back(propagateTransmission(reflectionPos[i].x, reflectionPos[i].y, xRays[i], quarterWaveCoating) + propagateTransmission(reflectionPos[i].x, reflectionPos[i].y, yRays[i], quarterWaveCoating));
	}
	return results;
}

std::vector<glm::vec3> LensSystem::getTransmission(std::vector<glm::vec2> reflectionPos, glm::vec2 xRay, glm::vec2 yRay, bool quarterWaveCoating) const {
	std::vector<glm::vec3> results;
	for (int i = 0; i < reflectionPos.size(); i++) {
		results.push_back(propagateTransmission(reflectionPos[i].x, reflectionPos[i].y, xRay, quarterWaveCoating) + propagateTransmission(reflectionPos[i].x, reflectionPos[i].y, yRay, quarterWaveCoating));
	}
	return results;
}

std::vector<glm::vec2> LensSystem::getPathIncidentAngleAtReflectionPos(glm::vec2 reflectionPair, glm::vec2 yawAndPitch) {

	glm::mat2x2 propagationMatrix = glm::mat2(1.0f);
	RayTransferMatrixBuilder rayTransferMatrixBuilder;
	glm::vec2 propagated_ray_x = glm::vec2(0, yawAndPitch.x);
	glm::vec2 propagated_ray_y = glm::vec2(0, yawAndPitch.y);

	glm::vec2 angles_first_interface(0);
	glm::vec2 angles_second_interface(0);
	std::vector<glm::vec2> res;

	//check if reflection makes sense
	if (reflectionPair.x > reflectionPair.y && reflectionPair.y >= 0 && reflectionPair.x < m_lens_interfaces.size()) {

		for (int i = 0; i < reflectionPair.x; i++) {
			if (i == 0) {
				propagationMatrix = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, 1.f, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * propagationMatrix;
			}
			else {
				propagationMatrix = rayTransferMatrixBuilder.getTranslationRefractionMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * propagationMatrix;
			}
		}
		///get first incident angle here
		angles_first_interface.x = (propagationMatrix * propagated_ray_x).y;
		angles_first_interface.y = (propagationMatrix * propagated_ray_y).y;

		propagationMatrix = rayTransferMatrixBuilder.getReflectionMatrix(m_lens_interfaces[reflectionPair.x].Ri) * propagationMatrix; //reflection step
		for (int i = reflectionPair.x - 1; i > reflectionPair.y; i--) {
			propagationMatrix = rayTransferMatrixBuilder.getinverseRefractionBackwardsTranslationMatrix(m_lens_interfaces[i].di, m_lens_interfaces[i - 1].ni, m_lens_interfaces[i].ni, m_lens_interfaces[i].Ri) * propagationMatrix;
		}
		propagationMatrix = rayTransferMatrixBuilder.getTranslationMatrix(m_lens_interfaces[reflectionPair.x].di) * propagationMatrix;
		///get second incident angle
		angles_second_interface.x = (propagationMatrix * propagated_ray_x).y;
		angles_second_interface.y = (propagationMatrix * propagated_ray_y).y;

	}

	res.push_back(angles_first_interface);
	res.push_back(angles_second_interface);
	return res;

}



