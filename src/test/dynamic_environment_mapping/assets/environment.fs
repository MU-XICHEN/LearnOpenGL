#version 330 core
in vec3 WorldPos;
in vec3 Normal;
out vec4 FragColor;
uniform samplerCube environmentMap;
uniform vec3 cameraPos;
uniform int mode; // 0 reflection, 1 refraction, 2 Fresnel mix
uniform float ior;
void main()
{
    // Incident direction, surface normal and cubemap axes all use WORLD space.
    vec3 I = normalize(WorldPos - cameraPos);
    vec3 N = normalize(Normal);
    float eta = 1.0 / ior; // air -> glass
    if (dot(I, N) > 0.0) { N = -N; eta = ior; }
    vec3 reflectedDir = reflect(I, N);
    vec3 refractedDir = refract(I, N, eta);
    bool totalInternalReflection = dot(refractedDir, refractedDir) < 0.00001;
    if (totalInternalReflection) refractedDir = reflectedDir;
    vec3 reflected = texture(environmentMap, reflectedDir).rgb;
    vec3 refracted = texture(environmentMap, refractedDir).rgb;
    float f0 = pow((ior - 1.0) / (ior + 1.0), 2.0);
    float fresnel = f0 + (1.0 - f0) * pow(1.0 - max(dot(-I, N), 0.0), 5.0);
    if (totalInternalReflection) fresnel = 1.0;
    vec3 color = mode == 0 ? reflected
               : mode == 1 ? refracted : mix(refracted, reflected, fresnel);
    FragColor = vec4(color, 1.0);
}
