#include "ray_transfer_matrices.h"

RayTransferMatrixBuilder::RayTransferMatrixBuilder() {

}

glm::mat2x2 RayTransferMatrixBuilder::getTranslationMatrix(float di) {
	return glm::mat2x2(1.0f, 0.0f, di, 1.0f);
}
glm::mat2x2 RayTransferMatrixBuilder::getRefractionMatrix(float n1, float n2, float Ri) {
	if (Ri == 0) {
		Ri = std::numeric_limits<float>::infinity();
	}
	float thirdTerm = (n1 - n2) / (n2 * Ri);
	float fourthTerm = n1 / n2;
	return glm::mat2x2(1.0f, thirdTerm, 0.0f, fourthTerm);
}
glm::mat2x2 RayTransferMatrixBuilder::getReflectionMatrix(float Ri) {
	if (Ri == 0) {
		Ri = std::numeric_limits<float>::infinity();
	}
	return glm::mat2x2(1.0f, (2.0f / Ri), 0.0f, 1.0f);
}
glm::mat2x2 RayTransferMatrixBuilder::getTranslationRefractionMatrix(float di, float n1, float n2, float Ri) {
	if (Ri == 0) {
		Ri = std::numeric_limits<float>::infinity();
	}
	return getTranslationMatrix(di) * getRefractionMatrix(n1, n2, Ri);
}
glm::mat2x2 RayTransferMatrixBuilder::getinverseRefractionBackwardsTranslationMatrix(float di, float n1, float n2, float Ri) {
	if (Ri == 0) {
		Ri = std::numeric_limits<float>::infinity();
	}
	return getRefractionMatrix(n2, n1, -Ri) * getTranslationMatrix(di);
}


