#version 450 core

layout(location = 1) uniform vec3 color;

layout(location = 6) uniform float entrance_pupil_height;
layout(location = 7) uniform sampler2D texApt;

layout(location = 9) uniform float irisApertureHeight;
layout(location = 10) uniform int cursorPosX;
layout(location = 11) uniform int cursorPosY;
layout(location = 12) uniform int quadID;
layout(location = 13) uniform bool getGhostsAtMouse;

struct QuadData {
    int quadID;
    float intensityVal;
};

layout(std430, binding = 0) buffer QuadIDBuffer {
    QuadData quadData[];
};

layout(binding = 2, offset = 0) uniform atomic_uint quadIDCounter;

layout(location = 0) out vec4 outColor;

in vec2 entrancePos;
in vec2 aptPos;
in float intensityVal;

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
    
    // Collect all ghosts at the mouse location, record the ghost id and intensityVal to sort.
    if (getGhostsAtMouse) {
        ivec2 fragCoord = ivec2(gl_FragCoord.xy);
        if (fragCoord.x == cursorPosX && fragCoord.y == cursorPosY) {
            // Atomically increment the counter and get the index
            uint index = atomicCounterIncrement(quadIDCounter);
            if (index < quadData.length()) {
                quadData[index].quadID = quadID;
                quadData[index].intensityVal = intensityVal;
            }
        }
    }

    outColor = vec4(color * intensityVal, 0.5);
}
