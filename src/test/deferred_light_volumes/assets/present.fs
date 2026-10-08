#version 330 core
out vec4 FragColor;
uniform sampler2D hdrLighting;
uniform float exposure;
void main()
{
    vec3 hdr = texelFetch(hdrLighting, ivec2(gl_FragCoord.xy), 0).rgb;
    // 先在浮点 FBO 中加法累积所有灯，再统一色调映射；避免逐灯写入 8-bit 颜色时截断。
    vec3 mapped = vec3(1.0) - exp(-hdr * exposure);
    FragColor = vec4(pow(mapped, vec3(1.0 / 2.2)), 1.0);
}
