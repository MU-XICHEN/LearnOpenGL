#version 330 core
in vec2 uv;
out vec4 FragColor;
uniform samplerCube capturedMap;
uniform int face;
void main()
{
    vec2 p = uv * 2.0 - 1.0;
    vec3 d;
    if      (face == 0) d = vec3( 1, -p.y, -p.x);
    else if (face == 1) d = vec3(-1, -p.y,  p.x);
    else if (face == 2) d = vec3( p.x,  1,  p.y);
    else if (face == 3) d = vec3( p.x, -1, -p.y);
    else if (face == 4) d = vec3( p.x, -p.y,  1);
    else                d = vec3(-p.x, -p.y, -1);
    FragColor = texture(capturedMap, d);
}
