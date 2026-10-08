#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    // 球体只是光栅化代理，不是被照亮的物体，因此不需要法线或 UV。
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
