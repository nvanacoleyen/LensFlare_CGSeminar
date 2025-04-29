#version 450 core

// Model/view/projection matrix
layout(location = 0) uniform mat4 mvp;
layout(location = 2) uniform mat4 sensor_matrix;

// Per-vertex attributes
layout(location = 0) in vec2 pos; // World-space position

void main() {
	// Transform 3D position into on-screen position
    gl_Position = mvp * sensor_matrix * vec4(pos, 50, 1.0);

}