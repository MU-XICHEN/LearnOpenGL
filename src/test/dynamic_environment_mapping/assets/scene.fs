#version 330 core
in vec3 WorldPos;
in vec3 Normal;
out vec4 FragColor;
uniform vec3 baseColor;
uniform bool floorPattern;
void main()
{
    vec3 color = baseColor;
    if (floorPattern)
    {
        float cell = mod(floor(WorldPos.x) + floor(WorldPos.z), 2.0);
        color = mix(vec3(0.12, 0.17, 0.23), vec3(0.46, 0.54, 0.61), cell);
    }
    float diffuse = max(dot(normalize(Normal), normalize(vec3(-0.4, 1.0, 0.6))), 0.0);
    FragColor = vec4(color * (0.35 + 0.65 * diffuse), 1.0);
}
