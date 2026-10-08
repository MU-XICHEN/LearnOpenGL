#version 330 core
out vec4 FragColor;
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform vec3 lightPosition;
uniform vec3 lightColor;
uniform float lightRadius;
uniform float linear;
uniform float quadratic;
uniform vec3 viewPos;
void main()
{
    // 注意：不能用球面的 UV 或世界坐标！当前屏幕像素对应的是 G-buffer 中的场景表面。
    // FBO 与窗口始终同尺寸，texelFetch 避免滤波以及归一化 UV 的舍入误差。
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec4 position = texelFetch(gPosition, pixel, 0);
    if (position.a == 0.0) discard;
    vec3 toLight = lightPosition - position.xyz;
    float distanceSquared = dot(toLight, toLight);
    // 球的投影内部仍可能包含球外的场景表面；这个判断负责正确性。
    // 性能收益来自球体投影外根本不启动本盏灯的片元着色，而不是依赖这个分支。
    if (distanceSquared >= lightRadius * lightRadius) discard;
    float distanceToLight = sqrt(distanceSquared);
    vec3 lightDir = toLight / max(distanceToLight, 0.0001);
    vec3 normal = normalize(texelFetch(gNormal, pixel, 0).xyz);
    vec4 material = texelFetch(gAlbedoSpec, pixel, 0);
    vec3 toView = viewPos - position.xyz;
    vec3 viewDir = toView / max(length(toView), 0.0001);
    vec3 halfway = lightDir + viewDir;
    halfway /= max(length(halfway), 0.0001);
    vec3 diffuse = max(dot(normal, lightDir), 0.0) * material.rgb * lightColor;
    vec3 specular = pow(max(dot(normal, halfway), 0.0), 16.0) * material.a * lightColor;
    float attenuation = 1.0 / (1.0 + linear * distanceToLight + quadratic * distanceSquared);
    // 没有 lights[]、没有光源循环、没有环境光：一次 draw 只输出一盏灯的贡献。
    FragColor = vec4((diffuse + specular) * attenuation, 0.0);
}
