#version 450
layout(location = 0) uniform mat4 projection;
layout(location = 1) uniform vec2 pos;

layout(location = 0) in vec2 aPos;

void main()
{
    gl_Position = projection * vec4(aPos, 0.0, 1.0);;
}