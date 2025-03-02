#version 450 core

layout(location = 2) uniform vec4 color;
layout(location = 3) uniform float entrance_pupil_height;
layout(location = 4) uniform sampler2D texApt;

layout(location = 0) out vec4 outColor;

in vec2 entrancePos;
in vec2 aptPos;

void main()
{

	// Entrance Clipping
    float distToOpticalAxis = sqrt(pow(entrancePos.x, 2.0) + pow(entrancePos.y, 2.0));
    if (distToOpticalAxis >= entrance_pupil_height) {
        discard;
    }

    // Apply aperture texture to get the aperture shape
    float apt_test = texture(texApt, aptPos).x;
    if (apt_test == 0.0) {
        discard;
    }

	outcolor = color;
}