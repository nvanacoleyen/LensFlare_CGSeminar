#version 450 core

layout(location = 0) uniform mat4 mvp;
layout(location = 1) uniform vec3 color;
layout(location = 2) uniform mat2 Ma;
layout(location = 3) uniform mat2 Ms;
layout(location = 4) uniform float light_angle_x;
layout(location = 5) uniform float light_angle_y;
layout(location = 7) uniform sampler2D texApt;
layout(location = 8) uniform mat4 sensorMatrix;
layout(location = 9) uniform float irisApertureHeight;
layout(location = 12) uniform int quadID;
layout(location = 14) uniform bool resetAnnotations;

struct AnnotationData {
    int quadID;
    float quadHeight;
    float aptHeight;
    int padding;
    vec2 quadCenterPos;
    vec2 aptCenterPos;
    vec4 quadColor;
};

layout(std430, binding = 1) buffer AnnotationBuffer {
    AnnotationData annotationData[];
};

layout(binding = 3, offset = 0) uniform atomic_uint annotationCounter;

layout(location = 0) in vec3 pos;

out vec2 entrancePos;
out vec2 aptPos;
out float intensityVal;

void main()
{
    float x_offset = pos.x;
    float y_offset = pos.y;

    entrancePos = pos.xy;

    vec2 ray_x = vec2(x_offset, light_angle_x);
    vec2 ray_y = vec2(y_offset, -light_angle_y);

    vec2 ray_x_a = Ma * ray_x;
    vec2 ray_y_a = Ma * ray_y;

    aptPos = (vec2(ray_x_a.x, ray_y_a.x) / irisApertureHeight) + vec2(0.5, 0.5);

    vec2 ray_x_s = Ms * ray_x_a;
    vec2 ray_y_s = Ms * ray_y_a;

    //QUAD CENTER
    vec2 quad_center_x = vec2(0.0, light_angle_x);
    vec2 quad_center_y = vec2(0.0, -light_angle_y);

    vec2 quad_center_x_s = Ms * Ma * quad_center_x;
    vec2 quad_center_y_s = Ms * Ma * quad_center_y;

    float quad_height = sqrt(pow((ray_x_s.x - quad_center_x_s.x), 2.0) + pow((ray_y_s.x - quad_center_y_s.x), 2.0));
    float initial_quad_height = sqrt(pow(x_offset, 2.0) + pow(y_offset, 2.0));
    intensityVal = initial_quad_height / quad_height;

    // Record all ghost details for annotations
    if (resetAnnotations) {
        // Atomically increment the counter and get the index
        uint index = atomicCounterIncrement(annotationCounter);
        if (index < annotationData.length()) {
            vec2 quad_center_pos = vec2(quad_center_x_s.x, quad_center_y_s.x);
            vec2 quad_center_x_a = Ma * quad_center_x;
            vec2 quad_center_y_a = Ma * quad_center_y;
            vec2 quad_center_apt_pos = (vec2(quad_center_x_a.x, quad_center_y_a.x) / irisApertureHeight) + vec2(0.5, 0.5);
            float quad_apt_height = sqrt(pow((aptPos.x - quad_center_apt_pos.x), 2.0) + pow((aptPos.y - quad_center_apt_pos.y), 2.0));

            annotationData[index].quadID = quadID;
            annotationData[index].quadHeight = quad_height;
            annotationData[index].quadCenterPos = quad_center_pos;
            annotationData[index].quadColor = vec4(color * intensityVal, 0.5);
            annotationData[index].aptCenterPos = quad_center_apt_pos;
            annotationData[index].aptHeight = quad_apt_height;
        }
    }


    gl_Position = mvp * sensorMatrix * vec4(vec3(ray_x_s.x, ray_y_s.x, 30.0), 1.0);
}
