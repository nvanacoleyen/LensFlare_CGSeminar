#version 450 core

layout(location = 1) uniform vec3 color;
layout(location = 4) uniform sampler2D texStarburst;


in vec2 TexCoord;

out vec4 outColor;

void main()
{
    outColor = vec4(color * texture(texStarburst, TexCoord).x, 0.5);
}