#pragma once

#include <glm/mat2x2.hpp>


class RayTransferMatrixBuilder {
public:
	RayTransferMatrixBuilder();
	glm::mat2x2 getTranslationMatrix(float di);
	glm::mat2x2 getRefractionMatrix(float n1, float n2, float Ri);
	glm::mat2x2 getReflectionMatrix(float Ri);
	glm::mat2x2 getTranslationRefractionMatrix(float di, float n1, float n2, float Ri);
	glm::mat2x2 getinverseRefractionBackwardsTranslationMatrix(float di, float n1, float n2, float Ri);
};
	
