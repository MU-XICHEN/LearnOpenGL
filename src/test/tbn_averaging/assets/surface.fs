#version 330 core
in vec3 WorldPos;
in vec3 N;
in vec3 T;
in vec3 B;
in vec2 UV;
out vec4 FragColor;
uniform sampler2D normalMap;
uniform vec3 lightPos;
uniform vec3 cameraPos;
uniform int mode;          // 0 face T/B, 1 averaged, 2 averaged error, 3 corrected
uniform int normalStyle;   // 0 texture, 1 constant tilt, 2 flat normal
uniform bool errorView;
uniform bool wire;
void main()
{
    if (wire) { FragColor = vec4(0.12,0.16,0.2,1); return; }
    vec3 n = normalize(N), t = normalize(T), b = normalize(B);
    if (mode == 3)
    {
        // 保留 UV 坐标系的手性；实际资产必须在镜像 UV 边界拆分顶点。
        float handedness = dot(cross(n,t),b) < 0.0 ? -1.0 : 1.0;
        // 插值后再做 Gram-Schmidt，避免顶点处正交、片元处又失去正交。
        t = normalize(t - n * dot(n,t));
        b = handedness * normalize(cross(n,t));
    }
    float error = max(max(abs(dot(t,n)),abs(dot(b,n))),abs(dot(t,b)));
    if (mode == 2 || errorView)
    {
        // 相同色标用于各阶段；显示真实点积误差，只对颜色映射放大。
        float e = clamp(error / 0.10,0.0,1.0);
        vec3 low = vec3(0.035,0.17,0.30), mid = vec3(0.98,0.65,0.08), high = vec3(0.94,0.12,0.16);
        FragColor = vec4(e < 0.5 ? mix(low,mid,e*2.0) : mix(mid,high,e*2.0-1.0),1.0);
        return;
    }
    vec3 tangentNormal = normalize(texture(normalMap,UV).rgb*2.0-1.0);
    if (normalStyle == 1) tangentNormal = normalize(vec3(0.65,0.35,1.0));
    if (normalStyle == 2) tangentNormal = vec3(0,0,1);
    // 归一化只能修正长度，不能消除非正交基引起的方向偏差。
    vec3 worldNormal = normalize(mat3(t,b,n)*tangentNormal);
    vec3 L = normalize(lightPos-WorldPos), V = normalize(cameraPos-WorldPos);
    float diffuse = max(dot(worldNormal,L),0.0);
    float specular = diffuse > 0.0 ? pow(max(dot(worldNormal,normalize(L+V)),0.0),48.0) : 0.0;
    vec3 color = vec3(0.10,0.34,0.46)*(0.16+0.84*diffuse) + vec3(0.95,0.82,0.6)*specular;
    FragColor = vec4(pow(color,vec3(1.0/2.2)),1.0);
}
