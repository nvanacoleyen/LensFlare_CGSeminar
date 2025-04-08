#pragma once

#include "lens_system.h"

void optimizeLensCoatingsSimple(LensSystem& lensSystem, glm::vec3 color, glm::vec2 reflectionPair);
void optimizeLensCoatingsBruteForce(LensSystem& lensSystem, glm::vec3 desiredColor, glm::vec2 reflectionPair, glm::vec2 yawAndPitch);
