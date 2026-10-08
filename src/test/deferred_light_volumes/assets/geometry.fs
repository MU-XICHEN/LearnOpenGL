#version 330 core
layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;
in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform bool textured;
void main()
{
    // alpha=1 表示这里有几何体；清屏时 alpha=0，避免背景也参与光照。
    gPosition = vec4(FragPos, 1.0);
    gNormal = normalize(Normal);
    gAlbedoSpec = textured
        ? vec4(texture(texture_diffuse1, TexCoords).rgb,
               texture(texture_specular1, TexCoords).r)
        : vec4(0.3, 0.32, 0.35, 0.25);
}
