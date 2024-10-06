#include "ray_transfer_matrices.h"

RayTransferMatrixBuilder::RayTransferMatrixBuilder() {

}

glm::mat2x2 RayTransferMatrixBuilder::getTranslationMatrix(float di) {
	return glm::mat2x2(1.0f, di, 0.0f, 1.0f);
}
glm::mat2x2 RayTransferMatrixBuilder::getRefractionMatrix(float n1, float n2, float Ri) {
	float thirdTerm = (n1 - n2) / (n2 * Ri);
	float fourthTerm = n1 / n2;
	return glm::mat2x2(1.0f, 0.0f, thirdTerm, fourthTerm);
}
glm::mat2x2 RayTransferMatrixBuilder::getReflectionMatrix(float Ri) {
	return glm::mat2x2(1.0f, 0.0f, (2.0f / Ri), 1.0f);
}
glm::mat2x2 RayTransferMatrixBuilder::getTranslationRefractionMatrix(float di, float n1, float n2, float Ri) {
	return getTranslationMatrix(di) * getRefractionMatrix(n1, n2, Ri);
}


