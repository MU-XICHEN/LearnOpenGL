#version 330 core
out vec4 FragColor;
uniform sampler2D gPosition;
uniform sampler2D gAlbedoSpec;
void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    if (texelFetch(gPosition, pixel, 0).a == 0.0)
        FragColor = vec4(0.015, 0.02, 0.03, 1.0);
    else
        FragColor = vec4(texelFetch(gAlbedoSpec, pixel, 0).rgb * 0.1, 1.0);
    // 环境光只画一次，否则会随着光源数量增加而错误变亮。
}
