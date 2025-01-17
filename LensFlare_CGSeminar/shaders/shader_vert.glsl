#version 450 core

layout(location = 0) uniform mat4 mvp;

layout(location = 2) uniform mat2 Ma;
layout(location = 3) uniform mat2 Ms;
layout(location = 4) uniform float light_angle_x;
layout(location = 5) uniform float light_angle_y;
layout(location = 6) uniform float entrance_pupil_height;
layout(location = 7) uniform sampler2D texApt;
layout(location = 8) uniform mat4 sensorMatrix;
layout(location = 9) uniform float irisApertureHeight;


layout(location = 0) in vec3 pos;

out vec2 entrancePos;
out vec2 aptPos;

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

    vec2 ray_x_s = Ms * ray_x;
    vec2 ray_y_s = Ms * ray_y;

    gl_Position = mvp * sensorMatrix * vec4(vec3(ray_x_s.x, ray_y_s.x, 30.0), 1.0);
}
