// 37ACUI - 基于 C++ / ImGui 的桌面管理工具
// 用于管理和运行 37AC 项目的 Python 服务端和 CLI 客户端

#include "gui/app.h"

int main(int, char **)
{
    App app;

    if (!app.init("37ACUI", 1280, 800))
    {
        return 1;
    }

    app.run();
    return 0;
}