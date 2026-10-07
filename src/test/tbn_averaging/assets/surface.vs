#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
layout(location=3) in vec3 triangleT;
layout(location=4) in vec3 triangleB;
layout(location=5) in vec3 averageT;
layout(location=6) in vec3 averageB;
uniform mat4 view;
uniform mat4 projection;
uniform int mode;
out vec3 WorldPos;
out vec3 N;
out vec3 T;
out vec3 B;
out vec2 UV;
void main()
{
    // 所有模式共享位置、UV、平滑法线，只改变 T/B 的处理。
    WorldPos = position;
    N = normal;
    T = mode == 0 ? triangleT : averageT;
    B = mode == 0 ? triangleB : averageB;
    UV = uv;
    gl_Position = projection * view * vec4(position,1.0);
}
