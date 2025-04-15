#version 450 core

layout(location = 0) uniform mat4 mvp;
layout(location = 1) uniform float irisApertureHeight;
layout(location = 2) uniform vec2 posAnnotationTransform;
layout(location = 3) uniform float sizeAnnotationTransform;
layout(location = 10) uniform mat4 sensorMatrix;

layout(location = 0) in vec3 pos;

out vec2 aptPos;
out float intensityVal;


void main()
{

    aptPos = (vec2(pos.x, pos.y) / irisApertureHeight) + vec2(0.5, 0.5);

    intensityVal = 1 / ((irisApertureHeight / 2) * sizeAnnotationTransform);

    gl_Position = (mvp * sensorMatrix * vec4(vec3((vec2(pos.x, pos.y) * sizeAnnotationTransform + posAnnotationTransform), 30.0), 1.0));
    
}
