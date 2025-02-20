#pragma once

#include "lens_system.h"

void optimizeLensCoatings(LensSystem& lensSystem, glm::vec3 desiredColor, glm::vec2 reflectionPair, glm::vec2 yawAndPitch);
void optimizeLensCoatingsBruteForce(LensSystem& lensSystem, glm::vec3 desiredColor, glm::vec2 reflectionPair, glm::vec2 yawAndPitch);
void optimizeLensCoatingsSimple(LensSystem& lensSystem, glm::vec3 color, glm::vec2 reflectionPair);
void optimizeLensCoatingsGradientDescent(LensSystem& lensSystem, glm::vec3 desiredColor, glm::vec2 reflectionPair, glm::vec2 yawAndPitch);
void optimizeLensCoatingsGradientDescent2(LensSystem& lensSystem, glm::vec3 desiredColor, glm::vec2 reflectionPair, glm::vec2 yawAndPitch);
void optimizeLensCoatingsBruteForce2(LensSystem& lensSystem, glm::vec3 desiredColor, glm::vec2 reflectionPair, glm::vec2 yawAndPitch);
