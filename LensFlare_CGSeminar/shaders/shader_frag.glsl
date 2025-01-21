#version 450 core


layout(location = 1) uniform vec3 color;




layout(location = 6) uniform float entrance_pupil_height;
layout(location = 7) uniform sampler2D texApt;

layout(location = 9) uniform float irisApertureHeight;

layout(location = 0) out vec4 outColor;

in vec2 entrancePos;
in vec2 aptPos;
in float intensityVal;

void main()
{
    float distToOpticalAxis = sqrt(pow(entrancePos.x, 2.0) + pow(entrancePos.y, 2.0));
    if (distToOpticalAxis >= entrance_pupil_height) {
        discard;
    }

    float apt_test = texture(texApt, aptPos).x;
    if (apt_test == 0.0) {
        discard;
    }

    outColor = vec4(color * intensityVal, 0.5);
}
