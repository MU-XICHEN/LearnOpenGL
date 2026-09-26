# 最小窗口示范

验证 GLFW 窗口、OpenGL 3.3 上下文和 GLAD 函数加载，作为后续示范的起点。

预期现象：控制台输出 OpenGL 版本，窗口显示蓝绿色背景。可以调整窗口大小，按 Esc 或关闭按钮退出。

在项目根目录执行：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug --target test__hello_window
Push-Location .\bin\test\Debug
.\test__hello_window.exe
Pop-Location
```
