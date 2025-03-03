#version 450 core

layout(location = 0) uniform mat4 mvp;
layout(location = 1) uniform mat4 sensorMatrix;


layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 entrancePos;
layout(location = 2) in vec2 aptPos;

out vec2 entrancePos;
out vec2 aptPos;

void main()
{
	gl_Position = mvp * sensorMatrix * vec4(pos, 1.0);
}