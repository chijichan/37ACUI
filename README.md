# 37AC 控制器 (37ACUI)

基于 **C++17 / Dear ImGui / OpenGL / GLFW** 的桌面管理工具，为 [37AC](https://github.com/chijichan/37AC) 提供图形化的模型、数据集、节点与服务端管理界面。

界面遵循 37AC Web 前端的设计系统「**樱花拿铁**」：唯一强调色樱花粉、暖灰文字、胶囊按钮、16px 卡片、柔光阴影与回弹动效（详见 [`src/gui/theme.h`](src/gui/theme.h)）。

## 功能

| 页面 | 能力 |
|------|------|
| **概览** | 环境 / 模型 / 类别数 / 检查点统计，快速开始，运行状态（进程、Flask 与节点端口在线状态），最近日志 |
| **模型** | 当前模型信息（版本、准确率、基模、SHA-256）、从头训练 / 继续训练、检查点列表（回滚 / 删除 / 清理）、训练基模切换、从 HF 下载基模、HF 登录、发布到 HF Hub |
| **数据集** | 原始 / 已裁剪数据集目录与状态、图像验证、YOLO 裁剪、压缩、转 JPEG |
| **节点** | 启动 / 停止识别节点、跳过模型自动更新、节点配置（ID / TCP / 服务端 / Token / 并发 / 心跳）、端口连通性探测 |
| **服务端** | 启动 / 停止 Flask + TCP 节点服务、端口状态、Web 站点 / 上传页 / 控制台 / 上传 API 快捷入口 |
| **环境** | Python 与 venv 状态、项目路径、一键初始化、创建 / 重建虚拟环境、安装依赖、环境校验、关键配置（LLM / 模型 / 数据集）、三份 `.env` 的生成 / 打开 / 删除 |
| **设置** | 中英文切换、程序 / 项目目录、源码下载地址、控制台视图与高度、系统信息、配置文件入口 |

底部**控制台**同时支持彩色（按级别着色的虚拟化日志）与纯文本（可框选复制）两种视图，命令可直接驱动 37AC CLI，进程运行时的输入会转发到子进程 stdin（可操作 37AC 的交互菜单）。

## 与 37AC 的协作方式

模型信息、检查点、基模、HF 与数据集操作不在 C++ 侧重复实现，而是通过桥接脚本调用 37AC 自身的 Python 服务，保持单一事实来源：

```
37ACUI (C++)
   │  读取 .env / 端口探测 / 进程管理（CreateProcess + 管道）
   ├── acui_bridge.py  ← 首次运行时自动写入程序目录（随 exe 分发，无需额外安装）
   │       └── services.checkpoint_service / services.hf_download / menu_service …
   └── main.py / runserver.py（CLI 与服务端进程，输出实时回显到控制台）
```

桥接脚本采用行式协议（`INFO` / `kind=value` / `CKPT` / `END`，动作返回 `OK` 或 `ERR=`），由 `src/gui/bridge.cpp` 解析；读取模型信息在后台线程执行，不阻塞界面。

## 系统托盘与低内存模式

窗口按钮的行为分开处理：

| 操作 | 默认行为 |
|---|---|
| **最小化（−）** | 最小化到任务栏（系统默认行为，界面资源保留，恢复立即） |
| **关闭（×）** | 隐藏到系统托盘（可在「设置 → 窗口 → 关闭时最小化到系统托盘」关闭，关闭后 × 直接退出） |
| 托盘图标双击 | 恢复主界面 |
| 托盘右键菜单 | 显示主界面 / 退出（退出始终结束程序） |

隐藏到托盘后任务栏条目消失，通知区出现 37ACUI 图标。

- 托盘图标：双击恢复；右键菜单；资源管理器重启后自动重新注册图标
- 进入托盘时释放全部 GUI 资源：
  - 销毁 ImGui 上下文、字体图集（GPU 纹理 + CPU 字形缓存）
  - 释放字体文件缓存（雅黑 / 雅黑 Bold 等，约 38MB）
  - 销毁 GLFW 窗口与 OpenGL 上下文（驱动为窗口/上下文分配的私有内存一并归还）
  - 收缩 CRT 堆并调用 `SetProcessWorkingSetSize` 归还工作集
- 隐藏期间完全休眠：不渲染、不跑动画、不做端口探测；训练/服务端等后台进程照常运行，日志继续写入缓冲但**不唤醒**主循环
- 恢复时重建窗口 / OpenGL 上下文 / ImGui / 字体（隐藏创建避免闪窗），并还原窗口位置、大小与最大化状态
- 托盘退出走正常关闭流程（保存配置、停止子进程）

实测（Windows 11，本机）：最小化后工作集约 **1.5MB**、提交内存约 36MB；界面常驻约 110–130MB。

> **字体内存优化**：ImGui 1.92 每次 `AddFontFromFileTTF` 都会把整份 TTF 读入内存，中文雅黑近 20MB，多字号会成倍占用。`theme.cpp` 改为自行读取文件一次，多个字号通过 `AddFontFromMemoryTTF(..., FontDataOwnedByAtlas=false)` 共享同一块数据，内存从 185MB 降到约 110–130MB。

## 设计系统对照

| 设计令牌 | Web 端 | 桌面端实现 |
|---|---|---|
| 强调色 | `--ac-pink-600 #C93470`（白字 5:1） | `Theme::T.pink600`，主按钮实心、悬停转 `pink500` |
| 文字 | `--ac-ink-900 #322931` 暖灰黑，禁纯黑 | 正文 `ink900`，辅助 `ink500`，禁用 `ink300` |
| 形状 | 按钮 / 徽章 pill，输入 12，卡片 16，模态 20 | `Button` 自绘胶囊，`FrameRounding 12`，`ChildRounding 16`，`PopupRounding 20` |
| 语义色 | success / warning / danger / info + 浅底 | 徽章、状态点、日志级别统一走 `UI::Tone` |
| 阴影 | `0 2px 12px rgba(50,41,49,.05)`，禁纯黑 | 卡片 / 浮层用多层圆角矩形叠加模拟柔光 |
| 动效 | `cubic-bezier(.34,1.56,.64,1)` 150–250ms | `Theme::anim()` 平滑趋近 + 回弹进程；有动画时主循环保持 60fps |
| 字体 | Nunito / JetBrains Mono | Segoe UI（合并微软雅黑）/ Cascadia Mono（woff2 无法被 stb_truetype 加载） |

## 技术栈

- **UI 框架**：Dear ImGui 1.92.9b（Docking 分支的输入/字体能力，未启用 viewport） + ImPlot（内置源码）
- **窗口 / OpenGL**：GLFW 3.4 + OpenGL 3.3 Core
- **进程管理**：Windows `CreateProcess` + 匿名管道（隐藏控制台、实时日志、stdin 转发）
- **端口探测**：Winsock 非阻塞 connect（Flask / 节点 TCP 在线状态）
- **国际化**：内置中 / 英切换（即时生效并写入配置）
- **构建**：CMake 3.16+，MSVC v143

## 构建

### 前置要求

- Visual Studio 2022（含「使用 C++ 的桌面开发」工作负载）
- CMake 3.16+
- vcpkg（安装 GLFW3）

```powershell
vcpkg install glfw3:x64-windows
```

### 编译

```cmd
build.bat
```

或手动：

```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=E:/apps/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

产物：`build/Release/ACUI.exe`（配置与桥接脚本会写入 exe 所在目录）。清理：`build.bat clean`。

## 使用说明

1. 启动 `ACUI.exe`。程序自动检测 Python 与 `.venv`，默认项目目录为 `<exe 目录>/project/37AC`（可在「设置」中改为已有的 37AC 仓库）。
2. 首次使用：在「概览」点击 **一键初始化**（下载源码 → 创建 `.venv` → 安装依赖 → 生成 `.env` → 校验环境）；或在「环境」页逐步执行。
3. 「模型」页读取 `src/cli/saves/models/config.json` 与检查点索引，可直接训练、回滚、切换基模或发布到 HF。
4. 「数据集」页执行验证 / YOLO 裁剪 / 压缩 / 转 JPEG，长任务输出到控制台。
5. 「服务端」「节点」页启动进程；端口状态每 4 秒探测一次，环形提示在线/离线。
6. 控制台命令：`help`、`train`、`resume`、`predict`、`node`、`server`、`verify`、`crop`、`compress`、`jpeg`、`model`、`info`、`clear`。

## 配置文件

| 文件 | 位置 | 说明 |
|---|---|---|
| `acui_settings.ini` | exe 目录 | 窗口位置与大小、语言、页面、控制台开关 / 视图 / 高度、数据集选择、节点选项、项目路径、下载地址（退出时保存） |
| `acui.ini` | exe 目录 | ImGui 窗口布局（退出时保存，避免周期性写盘） |
| `acui_bridge.py` | exe 目录 | 内嵌桥接脚本，版本与程序一致，内容变化时自动重写 |

## 项目结构

```
37ACUI/
├── CMakeLists.txt              # 构建配置（含 winmm / ws2_32）
├── build.bat                   # 一键构建 / 清理
├── 37ACUI.iss                  # Inno Setup 安装脚本
├── src/
│   ├── main.cpp                # 入口
│   ├── gui/
│   │   ├── app.h / app.cpp     # 窗口、配置、进程、应用外壳（侧栏 / 页头 / 控制台 / 状态栏 / 模态）
│   │   ├── pages.cpp           # 概览 / 模型 / 数据集 / 节点 / 服务端 / 环境 / 设置
│   │   ├── theme.h / theme.cpp # 设计令牌、字体、ImGui 样式、动效
│   │   ├── components.h/.cpp   # 卡片、胶囊按钮、徽章、开关、分段控件、Toast、图标
│   │   ├── bridge.h / bridge.cpp # 内嵌 Python 桥接脚本 + 行式协议解析
│   │   ├── tray.h / tray.cpp   # 系统托盘图标与菜单（独立 Win32 消息窗口）
│   │   ├── console.h/.cpp      # 彩色 / 纯文本日志视图与命令输入
│   │   └── lang.h / lang.cpp   # 中英文案表
│   └── network/
│       └── http_client.*       # WinHTTP 客户端（预留）
├── third_party/
│   ├── imgui/                  # Dear ImGui 源码
│   └── implot/                 # ImPlot 源码
└── resources/                  # 图标与资源
```

## 依赖

| 库 | 版本 | 用途 | 来源 |
|---|---|---|---|
| Dear ImGui | 1.92.9b | 即时模式 GUI | `third_party/imgui/`（内置） |
| ImPlot | 1.1 WIP | 图表（预留） | `third_party/implot/`（内置） |
| GLFW | 3.4 | 窗口 / OpenGL 上下文 | vcpkg `glfw3:x64-windows` |
| OpenGL | 3.3 Core | 渲染 | 系统 |
| WinHTTP | Windows SDK | HTTP 客户端 | 系统（`winhttp.lib`） |
| Winsock | Windows SDK | 端口探测 | 系统（`ws2_32.lib`） |

## 许可证

本项目采用 **GPL-3.0**（GNU General Public License v3）开源协议。

```text
Copyright (C) 2026 37ACUI Contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
```

完整协议文本见 [LICENSE](LICENSE)。

> 内置的 **Dear ImGui** 与 **ImPlot**（MIT License）保留各自许可证，MIT 与 GPL-3.0 兼容；37AC 项目遵循其自身许可证。
