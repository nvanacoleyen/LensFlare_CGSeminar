#version 450 core

layout(location = 0) uniform mat4 mvp;
layout(location = 2) uniform mat4 starburstMatrix;
layout(location = 3) uniform float scale;

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 texCoord;

out vec2 TexCoord;

void main()
{
    gl_Position = mvp * starburstMatrix * vec4((pos * scale), 1.0);
    TexCoord = texCoord;
}