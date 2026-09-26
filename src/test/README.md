# 知识点示范与快速验证

后续提出“示范：<知识点>”时，在这里新增可独立运行的程序。这里的 test 指学习示范，不是自动化单元测试。

## 已有示范

| 示范 | 构建目标 | 内容 |
| --- | --- | --- |
| [最小窗口](hello_window/README.md) | `test__hello_window` | 创建窗口并显示背景色 |
| [视口与附件尺寸](viewport_attachment_size/README.md) | `test__viewport_attachment_size` | 对照 viewport 与颜色附件尺寸不同时的结果 |
| [动态环境映射](dynamic_environment_mapping/README.md) | `test__dynamic_environment_mapping` | 用 FBO 更新 cubemap 六面，演示反射、折射和冻结捕获 |

## 目录约定

```text
src/test/
  CMakeLists.txt
  hello_window/
    main.cpp
    README.md
  <知识点英文 snake_case 名称>/
    main.cpp
    README.md
    assets/              # 可选：着色器、纹理等
```

每个直接子目录中的 `.cpp` 一起构成一个程序，必须且只能有一个 `main()`。头文件可使用 `.h` 或 `.hpp`。CMake 自动注册为 `test__<目录名>`；不需要修改根目录的章节列表。新增文件、目录或 assets 后重新执行配置命令。

默认使用仓库的 C++17、GLFW、GLAD、STB_IMAGE 等依赖。优先使用 OpenGL 3.3；需要更高版本时，在示范 README 中说明。

## 构建与运行

在项目根目录运行 PowerShell：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug --target test__hello_window
Push-Location .\bin\test\Debug
.\test__hello_window.exe
Pop-Location
```****
****
构建本目录全部示范（不构建其余教程）：

```powershell
cmake --build build --config Debug --target test_demos --parallel
```

普通全量构建也会包含这些示范。Windows/Visual Studio 输出为 `bin/test/Debug/`、`bin/test/Release/`，与其他模块一样保留配置子目录；单配置生成器输出为 `bin/test/`。

## 资源与调试

`assets/` 内容会在每次构建该示范时复制到可执行文件旁的 `<目录名>/`，避免不同示范的同名着色器覆盖。例如 `hello_window/assets/example.vs` 在运行时使用 `hello_window/example.vs` 访问。构建前新增 assets 目录需要重新配置。共享资源可以用 `FileSystem::getPath("resources/...")` 定位。

从可执行文件所在目录启动程序；Visual Studio 的调试工作目录已经自动设置。`bin/` 和 `build/` 继续由项目 `.gitignore` 忽略。

每个示范 README 应包含知识点、预期现象、操作方法、独立构建与运行命令。现有 `hello_window` 是可复制的最小窗口示例。
