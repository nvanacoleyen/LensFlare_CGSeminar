#pragma once

#include <glm/glm.hpp>
#include "quad.h"
#include "lens_system.h"

class LensBuilder {
public:
	LensBuilder();
	std::vector<SnapshotData> getAnnotationSnapshotData();
	void addGhost();
	void removeGhost(int ghostID);
	void setGhostPosition(int ghostID, glm::vec2 newPos);
	void setGhostSize(int ghostID, float newSize);
	LensSystem build(float light_angle_x, float light_angle_y);

private:
	std::vector<SnapshotData> m_annotationSnapshotData;
	int ghostIDCounter = 0;
};
