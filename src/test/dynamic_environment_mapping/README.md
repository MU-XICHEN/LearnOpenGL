# 动态环境映射：Framebuffer + Cubemap

把物体周围的场景实时渲染到 cubemap 的六个面，再让中央球体用这张 cubemap 进行反射或折射。使用现有 GLFW、GLAD、GLM、Shader 和天空盒资源，要求 OpenGL 3.3 Core。

## 构建与运行

在项目根目录执行 PowerShell：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug --target test__dynamic_environment_mapping --parallel
Push-Location .\bin\test\Debug
.\test__dynamic_environment_mapping.exe
Pop-Location
```

产物为 `bin/test/Debug/test__dynamic_environment_mapping.exe`。使用 Release 时，把构建配置和运行路径中的 `Debug` 都改为 `Release`。

本示例由 `src/test/CMakeLists.txt` 自动发现；`test_demos` 也会构建它。着色器自动复制到程序旁的 `dynamic_environment_mapping/`。天空盒复用仓库中的 `resources/textures/skybox/`，因此运行时仍需保留该目录；移动仓库后应重新配置并构建。

## 观察步骤

1. 默认是动态反射。中央镜面球会映出棋盘地面、立柱、天空，以及绕球移动的彩色方块。窗口底部展示六个捕获面的缩略图。
2. 按 `E` 改用静态天空盒：球面只有天空盒中的景物，当前场景的方块、地板和立柱不再出现在球面中。再按一次恢复动态贴图。
3. 在动态模式下按空格冻结捕获：方块继续运动，六张缩略图保持不变，球体仍采样冻结时的环境。移动相机时球面图案仍会变化，因为观察方向变了。
4. 恢复捕获，按 `2` 看折射，按 `3` 看 Fresnel 混合：球面正面以折射为主，掠射角处反射更明显。按 `1` 返回镜面反射。
5. 按 `P` 暂停物体运动，移动相机观察同一环境下的不同反射方向。它与空格不同：`P` 暂停场景时间，空格只停止更新 cubemap。

| 操作 | 功能 |
| --- | --- |
| `1` / `2` / `3` | 反射 / 折射 / Fresnel 混合 |
| `E` | 球体采样动态 cubemap / 静态天空盒 |
| 空格 | 冻结 / 恢复六面捕获 |
| `P` | 暂停 / 恢复物体运动 |
| `V` | 显示 / 隐藏六面缩略图 |
| `WASD`、鼠标右键拖动、滚轮 | 移动、转向、缩放视野 |
| `R` / `Esc` | 重置相机 / 退出 |

`E` 只切换球体采样的贴图；六面预览始终展示动态 cubemap，更新是否暂停由空格控制。

## 每帧做什么

```text
同一帧的场景状态
  ├─ 在球心向 +X、-X、+Y、-Y、+Z、-Z 各渲染一次
  │    每次：将对应面挂到 FBO → 清空颜色和深度 → 绘制周围场景
  │    得到完整的动态 cubemap
  └─ 切回窗口 framebuffer，恢复窗口 viewport
       绘制场景 → 绘制采样动态 cubemap 的球体 → 显示六面预览
```

在 [main.cpp](main.cpp) 中重点阅读：

- `Renderer()`：分配六个 `512×512` 颜色纹理面，以及一个同尺寸的深度 renderbuffer；检查每个面的 FBO 完整性。
- `attachFace()`：使用 `glFramebufferTexture2D(..., GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, ...)` 把某个面作为颜色附件。同一个 FBO 依次渲染六个面，无须创建六个 FBO。
- `captureEnvironment()`：六次捕获都位于球心 `(0,0,0)`，视场角 `90°`、宽高比 `1`，使用同一帧的场景时间。六个面共用深度缓冲，所以**每次换面都要清空深度**。
- `drawScene()`：绘制地板、方块、立柱和静态天空盒。捕获时不绘制中央球体，避免把自身写进环境贴图；天空盒始终采样静态贴图，避免一边写入一边采样同一张动态贴图。
- `drawMain()`：六个面全部完成后，切回默认 framebuffer，再让球体采样动态贴图。

捕获视口固定为 `512×512`，与 cubemap 单面及深度附件的尺寸一致。主视图使用 `glfwGetFramebufferSize()` 获取的实际像素尺寸，随窗口变化；切换 FBO 不会自动切换 viewport，代码会显式重设。

六个观察方向的 `up` 必须与 cubemap 的采样约定匹配：

| 面 | direction | up |
| --- | --- | --- |
| +X | (1, 0, 0) | (0, -1, 0) |
| -X | (-1, 0, 0) | (0, -1, 0) |
| +Y | (0, 1, 0) | (0, 0, 1) |
| -Y | (0, -1, 0) | (0, 0, -1) |
| +Z | (0, 0, 1) | (0, -1, 0) |
| -Z | (0, 0, -1) | (0, -1, 0) |

底部预览保留各纹理面的原始方向，因此侧面的地面会在上方；这与普通相机使用 `(0,1,0)` 作为 up 不同，不表示球面反射倒置。预览是最多 `160×160` 的缩略图，实际附件始终为 `512×512`。

## 球面如何采样

[assets/environment.fs](assets/environment.fs) 使用世界空间的位置、法线和观察方向：

```glsl
vec3 I = normalize(WorldPos - cameraPos); // 从相机射向表面
vec3 N = normalize(Normal);
vec3 reflectedDir = reflect(I, N);
vec3 refractedDir = refract(I, N, 1.0 / 1.52); // 空气进入玻璃
vec3 color = texture(environmentMap, reflectedDir).rgb;
```

反射和折射改变的是 cubemap 的采样方向。两者采样同一张已捕获的环境贴图，不需要分别捕获两套环境。模式 `3` 用 Schlick 近似根据观察角度混合反射与折射；着色器也处理了全反射时 `refract()` 返回零向量的情况。

这是单个捕获点的环境映射近似：近处物体可能存在位置偏差；玻璃模式没有追踪光线穿过球体的第二个界面、厚度或多次反弹，也不模拟自反射。每次更新增加六次场景绘制，实际项目可降低分辨率或更新频率。

## 运行验证

```powershell
Push-Location .\bin\test\Debug
.\test__dynamic_environment_mapping.exe --verify
Pop-Location
```

该模式创建隐藏窗口并在真实 OpenGL 上下文中渲染，验证六面有内容、方向产生不同图像、场景运动引起贴图变化、静态与动态反射不同、反射与折射及混合模式不同、主视图绘制不改写已捕获贴图，以及渲染后没有 OpenGL 错误。失败返回非零退出码。

通过时输出 `PASS`，并在 `bin/test/Debug/dynamic_environment_mapping/preview.ppm` 保存带六面预览的画面。该产物位于已忽略的 `bin/`，不会加入 Git。

## 对照原有教程

- [framebuffers.cpp](../../4.advanced_opengl/5.1.framebuffers/framebuffers.cpp)：颜色附件由普通 2D 纹理改为 cubemap 的一个面，保留 FBO 和深度附件的思路。
- [cubemaps_skybox.cpp](../../4.advanced_opengl/6.1.cubemaps_skybox/cubemaps_skybox.cpp)：复用天空盒资源、方向采样及天空盒深度处理；球体改为采样实时生成的环境贴图。
- [LearnOpenGL：Cubemaps](https://learnopengl.com/Advanced-OpenGL/Cubemaps)：环境映射、反射与折射的原教程说明。
