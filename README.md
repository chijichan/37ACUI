# 37AC 控制器 (37ACUI)

基于 **C++17 / ImGui / OpenGL / GLFW** 的桌面管理工具，为 [37AC](https://github.com/chijichan/37AC) 项目提供图形化的服务端管理和 CLI 操作界面。

## 功能

| 标签页 | 功能 |
|--------|------|
| **Git** | 从 GitHub 克隆 37AC 项目、选择目标目录、检查目录状态 |
| **环境** | 检测 Python 环境、安装 CLI/服务端依赖、创建虚拟环境、验证环境完整性 |
| **服务端** | 启动/停止 Flask 服务端、检查运行状态、快捷打开 Web 前端和 API 页面 |
| **CLI** | 模型训练/继续训练、角色预测、节点服务、图像验证、交互菜单 |
| **控制台** | 实时显示所有操作的输出日志，支持命令行输入和进程交互 |

## 技术栈

- **UI 框架**: Dear ImGui (Docking 分支) + ImPlot
- **窗口/OpenGL**: GLFW 3.x + OpenGL 3.3 Core
- **HTTP 客户端**: WinHTTP (Windows 原生)
- **进程管理**: Windows CreateProcess + 命名管道
- **国际化**: 内置中/英文切换 (UTF-8)
- **构建系统**: CMake 3.16+

## 快速开始

### 前置要求

- **Visual Studio 2022** (含"使用 C++ 的桌面开发"工作负载)
- **CMake** 3.16+
- **vcpkg** (用于安装 GLFW3)
- **Git** (用于克隆 37AC 项目)

### 安装依赖

```powershell
# 通过 vcpkg 安装 GLFW3
vcpkg install glfw3:x64-windows
```

### 构建

**方式 1: 使用构建脚本**

```cmd
build.bat
```

**方式 2: 手动 CMake 构建**

```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=E:/apps/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

构建产物位于 `build/Release/ACUI.exe`。

### 清理

```cmd
build.bat clean
```

## 使用说明

1. 启动 `ACUI.exe`，程序会自动检测 37AC 项目的 Python 虚拟环境
2. 如果尚未下载 37AC，在 **Git** 标签页输入仓库 URL 和目标目录进行克隆
3. 在 **环境** 标签页安装依赖、创建虚拟环境
4. 在 **服务端** 标签页管理 Flask 后端服务
5. 在 **CLI** 标签页进行模型训练、预测等操作
6. 底部控制台支持直接输入命令（如 `train`、`server`、`node` 等）

## 配置

程序会在运行目录生成两个配置文件：

- `acui.ini` — ImGui 窗口布局（退出时保存一次，不持续写盘）
- `acui_settings.ini` — 用户设置（窗口位置/大小、语言、项目路径、Git 配置，退出时保存）

## 项目结构

```
37ACUI/
├── CMakeLists.txt              # CMake 构建配置
├── build.bat                   # 一键构建/清理脚本
├── README.md                   # 项目说明
├── acui.ini                    # ImGui 窗口布局配置
├── src/
│   ├── main.cpp                # 程序入口
│   ├── gui/
│   │   ├── app.h               # App 主类声明
│   │   ├── app.cpp             # 主窗口、面板渲染、进程管理
│   │   ├── console.h           # 控制台控件声明
│   │   ├── console.cpp         # 控制台控件实现
│   │   ├── lang.h              # 国际化系统声明
│   │   ├── lang.cpp            # 国际化字符串定义
│   │   ├── panel_env.h         # 环境面板（占位头文件）
│   │   ├── panel_server.h      # 服务端面板（占位头文件）
│   │   └── panel_cli.h         # CLI 面板（占位头文件）
│   └── network/
│       ├── http_client.h       # WinHTTP 客户端声明
│       └── http_client.cpp     # WinHTTP 客户端实现
├── third_party/
│   ├── imgui/                  # Dear ImGui 源码
│   │   └── backends/           # GLFW + OpenGL3 后端
│   └── implot/                 # ImPlot 图表库源码
└── resources/                  # 资源文件目录
```

## 依赖

| 库 | 版本 | 用途 | 来源 |
|---|---|---|---|
| Dear ImGui | 1.92.9b | 即时模式 GUI | `third_party/imgui/` (内置源码) |
| ImPlot | 1.1 WIP | 图表绘制 | `third_party/implot/` (内置源码) |
| GLFW | 3.4 | 窗口/OpenGL 上下文 | vcpkg (`glfw3:x64-windows`) |
| OpenGL | 3.3 Core | 图形渲染 | 系统 (GLEW 由 ImGui GLFW/OpenGL3 后端内部加载) |
| WinHTTP | Windows SDK 内置 | HTTP 请求 (`winhttp.lib`) | 系统 |
| MSVC C++ 运行时 | VS2022 (v143) | C++17 标准库 | Visual Studio 2022 |

> **版本核对**：ImGui 1.92.9b 与 ImPlot 1.1 WIP 均以源码内置，版本定义见 `third_party/imgui/imgui.h`（`IMGUI_VERSION`）与 `third_party/implot/implot.h`（`IMPLOT_VERSION`）；GLFW 3.4 由 vcpkg 安装（`vcpkg list` 可查）。

## 许可证

本项目仅用于学习和研究目的。