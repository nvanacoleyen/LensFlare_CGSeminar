#version 450 core

layout(location = 0) uniform mat4 mvp;
layout(location = 1) uniform vec3 color;
layout(location = 2) uniform mat2 Ma;
layout(location = 3) uniform mat2 Ms;
layout(location = 4) uniform float light_angle_x;
layout(location = 5) uniform float light_angle_y;
layout(location = 6) uniform float entrance_pupil_height;
layout(location = 7) uniform sampler2D texApt;
layout(location = 8) uniform mat4 sensorMatrix;
layout(location = 9) uniform float irisApertureHeight;
layout(location = 12) uniform int quadID;
layout(location = 14) uniform int m_takeSnapshot;
layout(location = 15) uniform vec2 posAnnotationTransform;
layout(location = 16) uniform float sizeAnnotationTransform;
layout(location = 17) uniform bool disableEntranceClipping;

struct SnapshotData {
    int quadID;
    float quadHeight;
    vec2 quadCenterPos;
    vec4 quadColor;
};

layout(std430, binding = 1) buffer SnapshotBuffer {
    SnapshotData snapshotData[];
};

layout(binding = 3, offset = 0) uniform atomic_uint snapshotCounter;

layout(location = 0) in vec3 pos;

out vec2 entrancePos;
out vec2 aptPos;
out float intensityVal;

vec2 calculateInitialOffset(mat2 matrix, float y_initial) {
    float a = matrix[0][0]; // Top-left element
    float b = matrix[1][0]; // Top-right element

    // Check if 'a' is zero to avoid division by zero, 
    if (abs(a) < 1e-6) {
        return vec2(0.0, y_initial); // fallback
    }

    // Calculate the initial offset value
    float x_initial = -(b * y_initial) / a;

    return vec2(x_initial, y_initial); // Return as a 2D vector
}


void main()
{
    float x_offset = pos.x;
    float y_offset = pos.y;

    entrancePos = pos.xy;

    vec2 ray_x = vec2(x_offset, light_angle_x);
    vec2 ray_y = vec2(y_offset, light_angle_y);

    vec2 ray_x_a = Ma * ray_x;
    vec2 ray_y_a = Ma * ray_y;

    aptPos = (vec2(ray_x_a.x, ray_y_a.x) / irisApertureHeight) + vec2(0.5, 0.5);

    vec2 ray_x_s = Ms * ray_x_a;
    vec2 ray_y_s = Ms * ray_y_a;

    //FLARE CENTER, APT CENTER PROJECTED ON SENSOR
    vec2 quad_center_x = calculateInitialOffset(Ma, light_angle_x);
    vec2 quad_center_y = calculateInitialOffset(Ma, light_angle_y);

    vec2 quad_center_x_s = Ms * Ma * quad_center_x;
    vec2 quad_center_y_s = Ms * Ma * quad_center_y;
    vec2 quad_center_pos = vec2(quad_center_x_s.x, quad_center_y_s.x);

    vec2 apt_h_x = vec2(((irisApertureHeight / 2) - (light_angle_x * Ma[1][0])) / Ma[0][0], light_angle_x);
    vec2 apt_h_x_s = Ms * Ma * apt_h_x;
    float ghost_height = abs(apt_h_x_s.x - quad_center_pos.x);

    intensityVal = irisApertureHeight / (ghost_height * sizeAnnotationTransform);

    vec2 entrance_pupil_h_x_s;
    vec2 entrance_pupil_center_x_s;
    float entrance_pupil_height_s;

    if (!disableEntranceClipping) {
        entrance_pupil_h_x_s = Ms * Ma * vec2(entrance_pupil_height, light_angle_x);
        entrance_pupil_center_x_s = Ms * Ma * vec2(0.f, light_angle_x);
	    entrance_pupil_height_s = abs(entrance_pupil_h_x_s.x - entrance_pupil_center_x_s.x);
    }

    vec2 centerPos;
    float height;
    if (entrance_pupil_height_s < ghost_height && !disableEntranceClipping) {
        vec2 entrance_pupil_center_y_s = Ms * Ma * vec2(0.f, light_angle_y);
        centerPos = vec2(entrance_pupil_center_x_s.x, entrance_pupil_center_y_s.x);
        height = entrance_pupil_height_s;
    } else {
        centerPos = quad_center_pos;
        height = ghost_height;
    }

    // Record all ghost details without annotations
    if (m_takeSnapshot == 1) {
        uint index = atomicCounterIncrement(snapshotCounter);
        if (index < snapshotData.length()) {         
            snapshotData[index].quadCenterPos = centerPos;
            snapshotData[index].quadHeight = height;
            snapshotData[index].quadID = quadID;
            snapshotData[index].quadColor = vec4(color * intensityVal, 0.5);
        }
    } else if (m_takeSnapshot == 2) { //with annotations
        uint index = atomicCounterIncrement(snapshotCounter);
        if (index < snapshotData.length()) {
            snapshotData[index].quadCenterPos = centerPos + posAnnotationTransform;
            snapshotData[index].quadHeight = height * sizeAnnotationTransform;
            snapshotData[index].quadID = quadID;
            snapshotData[index].quadColor = vec4(color * intensityVal, 0.5);
        }
    }

    gl_Position = (mvp * sensorMatrix * vec4(vec3(((vec2(ray_x_s.x, ray_y_s.x) - centerPos) * sizeAnnotationTransform + centerPos + posAnnotationTransform), 50.0), 1.0));
    
}
