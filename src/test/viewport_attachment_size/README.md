# glViewport 与 framebuffer 纹理附件尺寸

`glViewport` 将透视除法后的 NDC x/y 映射到当前绘制 framebuffer 的像素坐标。它不分配纹理，也不会改变附件尺寸。附件尺寸由 `glTexImage2D` 等存储分配操作决定。

**二者不要求相同。** 想把完整画面填满整个附件时，通常设置为相同；分屏、局部渲染等场景可以故意使用不同尺寸。视口与附件大小不同本身不会导致 framebuffer 不完整，也通常不会产生 OpenGL 错误。

## 构建与运行

在项目根目录使用 PowerShell：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug --target test__viewport_attachment_size
Push-Location .\bin\test\Debug
.\test__viewport_attachment_size.exe
Pop-Location
```

- 按 **1/2/3** 切换 **400×300、800×600、1200×900** 的三个独立纹理附件，默认显示 800×600。
- 三种附件都从窗口 framebuffer 的 `(20,20)` 开始按 **1:1 像素尺寸**显示；深色背景是附件外的窗口区域。
- 默认三个离屏绘制 pass 都设置 `glViewport(0, 0, 800, 600)`。
- **空格**：在固定 800×600 与各自匹配附件尺寸之间切换。
- **Esc**：退出。窗口大小可调整，不改变离屏附件或固定视口尺寸。窗口太小时会裁切超出的部分，不会自动缩小；放大窗口可看到更多内容。
- 白线标出 NDC 外框、中心坐标轴和中心圆；红色随 x 增加、绿色随 y 增加。紫色是清屏颜色，表示这部分没有被图形覆盖。

## 预期现象

以下原点均为左下角，视口偏移为 `(0,0)`：

| 纹理附件 | 视口 | 附件中实际保存的内容 |
|---|---|---|
| 400×300（小于视口） | 800×600 | 仅容纳原画面左下四分之一；其余绘制落到附件范围之外，不会写入纹理。不会自动把完整画面缩小。 |
| 800×600（等于视口） | 800×600 | 完整画面恰好填满附件。 |
| 1200×900（大于视口） | 800×600 | 完整画面位于附件左下 800×600 区域；上方和右方保持紫色清屏结果。 |

显示阶段不缩放。`glBlitFramebuffer` 的源矩形和目标矩形宽高完全一致：

```cpp
glBlitFramebuffer(0, 0, t.width, t.height,
                  20, 20, 20 + t.width, 20 + t.height,
                  GL_COLOR_BUFFER_BIT, GL_NEAREST);
```

一个纹理像素对应一个窗口 framebuffer 像素（高 DPI 下不一定等于窗口逻辑坐标的一个单位）。`GL_NEAREST` 本身不能禁止缩放，真正保证 1:1 的是源/目标矩形尺寸相等。blit 不受 `glViewport` 控制。

固定视口时，按 1 看到的是未放大的左下局部；按 2 看到完整的 800×600 图案；按 3，图案仍是相同的 800×600 大小与位置，右方/上方多出紫色附件区域。三个附件相当于从相同坐标原点观察，方便直接比较边界。

按空格匹配尺寸后，三幅图都完整覆盖各自附件；小纹理的采样精度仍较低。

## 坐标计算

一般映射为：

```text
x_pixel = viewport_x + (x_ndc + 1) * viewport_width  / 2
y_pixel = viewport_y + (y_ndc + 1) * viewport_height / 2
```

当 viewport 为 `(0,0,800,600)`：

- NDC `(-1,-1)` → 边界 `(0,0)`。
- NDC `(0,0)` → `(400,300)`。
- NDC `(1,1)` → 边界 `(800,600)`。

像素中心为半整数坐标，例如左下像素中心 `(0.5,0.5)`。400×300 附件只具有索引 x=0…399、y=0…299 的像素，NDC 中心已经落在该附件的右上边界。

## 易混淆的状态

- `glBindFramebuffer` 不会自动更新视口：每个绘制 pass 按自己的目标设置，返回窗口时使用 `glfwGetFramebufferSize` 获取实际像素尺寸。
- `glClear` 不受视口限制。本例关闭 scissor，因此先把整个附件清成紫色。若不清屏，未被绘制区域可能保留旧内容；新分配且没有初始化的纹理内容不能依赖。
- `glViewport` 是坐标映射；若要显式限制清屏及写入的矩形，使用 `glScissor` 并开启 `GL_SCISSOR_TEST`。
- 本例只有一个颜色附件，不添加深度/模板附件，避免引入多附件尺寸等额外因素；混合、深度测试和 scissor 均未开启。

## 自动验证

```powershell
.\bin\test\Debug\test__viewport_attachment_size.exe --verify
```

此模式使用隐藏窗口创建真实 OpenGL 上下文，关闭装饰线，只保留可计算的坐标渐变。对三个附件分别在固定/匹配两种模式下读取 25 个像素（含视口边界两侧），验证裁切后的坐标值、未覆盖区域的清屏颜色与匹配后的完整覆盖。另外对每种情况，将显示阶段的 25 个可见窗口像素与源附件对应像素比较，验证 1:1 复制。输出十二条 PASS 且退出码为 0 表示验证通过；它不替代交互操作检查。

关键代码：`createTarget` 分配附件；`renderTarget` 设置视口与绘制；`showTarget` 按原始尺寸展示附件；`checkPixels` 验证离屏像素；`checkPresentation` 验证显示像素。

参考：[Khronos glViewport](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glViewport.xhtml)、[glClear](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glClear.xhtml)、[glBlitFramebuffer](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBlitFramebuffer.xhtml)。
