# 37AC 控制器

基于 **C++ / ImGui / OpenGL** 的桌面管理工具，用于管理和运行 [37AC](https://github.com/your/37AC) 项目的服务端和 CLI 客户端。

## 功能

| 标签页 | 功能 |
|--------|------|
| **环境状态** | 检测 Python 环境、安装依赖、创建虚拟环境、验证环境完整性 |
| **服务端** | 启动/停止 Flask 服务端、检查运行状态、快捷打开 Web 前端 |
| **CLI 客户端** | 训练模型、预测角色、启动节点服务、验证图像、交互菜单 |
| **运行日志** | 实时显示所有操作的输出日志 |

## 构建

### 前置要求

- **Visual Studio 2022** (含 C++ 桌面开发工作负载)
- **CMake** 3.16+
- **GLFW** 3.x (通过 vcpkg 或系统安装)

### 方式 1: 直接运行脚本

```bash
build.bat
```

### 方式 2: 手动 CMake

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## 使用

启动后程序会自动检测项目的 Python 环境（`.venv/Scripts/python.exe`），通过标签页切换管理不同功能。

## 项目结构

```
37ACUI/
├── CMakeLists.txt          # CMake 构建配置
├── build.bat               # 一键构建脚本
├── README.md
├── src/
│   ├── main.cpp            # 主入口
│   ├── gui/
│   │   ├── app.h           # App 主类
│   │   ├── app.cpp         # 主窗口、面板、渲染逻辑
│   │   ├── panel_env.h
│   │   ├── panel_server.h
│   │   └── panel_cli.h
│   └── network/
│       ├── http_client.h   # HTTP 客户端 (WinHTTP)
│       └── http_client.cpp
└── resources/