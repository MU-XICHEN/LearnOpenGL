#version 330 core
layout (location = 0) in vec3 aPos;
out vec3 direction;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    direction = aPos;
    // Remove camera translation, as in cubemaps_skybox.
    vec4 clip = projection * mat4(mat3(view)) * vec4(aPos, 1.0);
    gl_Position = clip.xyww; // depth = 1 after perspective division
}
