#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
uniform mat4 view;
uniform mat4 projection;
out vec3 Color;
void main() { Color=color; gl_Position=projection*view*vec4(position,1); }
