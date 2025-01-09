#version 450 core

// Model/view/projection matrix
layout(location = 0) uniform mat4 mvp;
layout(location = 2) uniform vec3 lightPos;

// Per-vertex attributes
layout(location = 0) in vec3 pos; // World-space position

void main() {
	// Transform 3D position into on-screen position
    gl_Position = mvp * vec4(pos + lightPos, 1.0);

}