#version 330 core
layout(location = 0) in vec2 pixel;
uniform vec2 screenSize;
void main() { gl_Position = vec4(pixel / screenSize * 2.0 - 1.0, 0, 1); }
