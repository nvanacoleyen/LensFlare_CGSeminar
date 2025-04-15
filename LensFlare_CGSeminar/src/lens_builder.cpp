#include "lens_builder.h"
#include "lens_solver.h"

LensBuilder::LensBuilder() {
}
std::vector<SnapshotData> LensBuilder::getAnnotationSnapshotData() {
	return m_annotationSnapshotData;
}

void LensBuilder::addGhost() {
	SnapshotData newGhost;
	newGhost.quadID = ghostIDCounter;
	newGhost.quadHeight = 1.f; //check if same "unit"
	newGhost.quadCenterPos = glm::vec2(0.f, 0.f); //check if same origin
	m_annotationSnapshotData.push_back(newGhost);
	ghostIDCounter += 1;
}

void LensBuilder::removeGhost(int ghostID) {
	for (auto it = m_annotationSnapshotData.begin(); it != m_annotationSnapshotData.end(); ++it) {
		if (it->quadID == ghostID) {
			m_annotationSnapshotData.erase(it);
			break;
		}
	}
}

void LensBuilder::setGhostPosition(int ghostID, glm::vec2 newPos) {
	for (auto& ghost : m_annotationSnapshotData) {
		if (ghost.quadID == ghostID) {
			ghost.quadCenterPos = newPos;
			break;
		}
	}
}

void LensBuilder::setGhostSize(int ghostID, float newSize) {
	for (auto& ghost : m_annotationSnapshotData) {
		if (ghost.quadID == ghostID) {
			ghost.quadHeight = newSize;
			break;
		}
	}
}

LensSystem LensBuilder::build(float light_angle_x, float light_angle_y) {
	// Build lens system implementation
	LensSystem lensSystem = solve_Annotations(m_annotationSnapshotData, light_angle_x, light_angle_y);
	return lensSystem;
}
