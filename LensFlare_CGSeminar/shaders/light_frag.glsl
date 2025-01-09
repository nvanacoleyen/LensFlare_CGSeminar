#version 450 core

layout(location = 1) uniform vec3 color;

// Output for on-screen color
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(color, 0.5);
}
