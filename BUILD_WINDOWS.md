# Windows 编译与运行指南

## 环境要求

- 64 位 Windows
- CMake 3.13 或更高版本
- Visual Studio，并安装 **使用 C++ 的桌面开发** 工作负载
- 支持相应 OpenGL 版本的显卡驱动

仓库已经包含 GLFW、GLAD、GLM、Assimp 和 FreeType 等依赖，无需单独下载。大部分教程需要 OpenGL 3.3，少数高级示例需要 OpenGL 4.1 至 4.5。

## 配置项目

在项目根目录打开 PowerShell：

```powershell
cd C:\Users\hengj\Desktop\workspace\LearnOpenGL
cmake -S . -B build -A x64
```

CMake 会自动选择已安装的新版 Visual Studio，并在 `build` 目录中生成解决方案。

## 编译并运行第一个示例

```powershell
cmake --build build --config Debug --target 1.getting_started__1.1.hello_window
.\bin\1.getting_started\Debug\1.getting_started__1.1.hello_window.exe
```

生成的程序按照章节存放在：

```text
bin\<章节>\Debug\
```

## 编译全部示例

```powershell
cmake --build build --config Debug --parallel
```

项目包含约 92 个示例。完整编译时，部分旧依赖和 guest 示例可能产生运行库冲突或浮点类型转换警告，但当前环境下可以完成构建。

## 使用 Visual Studio

打开生成的解决方案：

```powershell
start .\build\LearnOpenGL.sln
```

在 Visual Studio 中选择一个示例项目，右键设置为启动项目，然后按 `F5` 运行。项目已配置正确的工作目录，运行时所需的着色器和 DLL 会自动复制到程序目录。

## 清理和重新构建

清理后重新编译：

```powershell
cmake --build build --config Debug --clean-first
```

如果需要彻底重新生成构建目录：

```powershell
Remove-Item -Recurse -Force build
cmake -S . -B build -A x64
```

`build` 和 `bin` 均为可再生目录，已经在 `.gitignore` 中排除。
