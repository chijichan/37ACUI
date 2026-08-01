#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include "network/http_client.h"
#include "gui/console.h"
#include "gui/lang.h"

class App
{
public:
    App();
    ~App();

    bool init(const char *title, int width, int height);
    void run();
    void renderFrame(); // 渲染一帧（仅在有变化需要重绘时调用）

    HttpClient &http() { return m_http; }
    const std::string &projectRoot() const { return m_projectRoot; }
    const std::string &cliRoot() const { return m_cliRoot; }
    const std::string &serverRoot() const { return m_serverRoot; }

    void addLog(const std::string &msg, unsigned int = 0xFFFFFFFF);
    void addInfo(const std::string &msg) { addLog(msg); }
    void addSuccess(const std::string &msg) { addLog(msg); }
    void addError(const std::string &msg) { addLog(msg); }
    void addWarn(const std::string &msg) { addLog(msg); }

    bool runPython(const std::string &cmd, const std::string &cwd,
                   std::string &output, int &exitCode);
    bool checkPython();
    void createVenv(bool recreate); // 创建虚拟环境（recreate=true 时先删除旧 .venv）

    void startProcess(const std::string &name, const std::string &cmd,
                      const std::string &cwd);
    void stopProcess();
    bool isProcRunning() const { return m_procRunning; }

    void startGitClone(const std::string &repoUrl, const std::string &targetDir);
    void startInitProject(); // 一键初始化：克隆→检查→创建环境→安装依赖→验证环境

private:
    void render();
    void renderSideNav();     // 左侧导航栏
    void renderContentArea(); // 右侧内容区
    void renderStatusBar();
    void renderLogArea();     // 控制台区
    void renderLaunchPanel(); // 启动页（环境+Git+服务端+CLI）
    void renderEnvPanel();    // 环境 + Git
    void renderServerPanel();
    void renderCliPanel();
    void renderToolsPanel();    // 工具页
    void renderSettingsPanel(); // 设置页（语言）

    // 卡片布局：圆角 + 浅色背景 + 边框 + 加粗标题
    void beginCard(const char *id, const char *title, float width = -1.0f);
    void endCard();

    void procThreadFunc(const std::string &name, const std::string &cmd,
                        const std::string &cwd);
    void venvThreadFunc(bool recreate);                 // 后台线程：删除旧 .venv + 创建新环境
    void initProjectThreadFunc(const std::string &url); // 后台线程：一键初始化项目流程
    void closeProcStdin();                              // 线程安全：关闭并置空 m_procStdinWrite

    void saveConfig();
    void loadConfig();
    void updateProjectPaths(); // 根据 programDir 计算 project/cli/server 路径
    void refreshVenvState();   // 刷新 .venv 存在性缓存（避免每帧文件系统查询）
    void setPythonState(bool ok, const std::string &ver); // 线程安全：更新 Python 检测结果

    HttpClient m_http;
    std::string m_programDir; // 程序地址（init 时从 exe 路径自动获取）
    std::string m_projectRoot, m_cliRoot, m_serverRoot;
    int m_activeNav = 0; // 0=启动 1=工具 2=设置
    bool m_showDemo = false, m_pythonOk = false;
    std::string m_pythonVersion;
    std::atomic<bool> m_procRunning{false};
    std::thread m_procThread;
    std::thread m_pingThread; // 服务端状态检查线程（避免同步 HTTP 阻塞 UI）
    ConsoleWidget m_console;
    std::mutex m_procStdinMutex;             // 保护 m_procStdinWrite：主线程（发送/停止）与 worker 线程（设置/清理）并发
    void *m_procStdinWrite = nullptr;
    std::mutex m_stateMutex;                 // 保护跨线程共享的状态成员：路径、Python 状态、venv/Git 状态
    std::atomic<int> m_serverStatus{-1};
    std::atomic<unsigned long> m_procPid{0}; // 当前子进程 PID（0 = 无），用于精确终止进程
    std::atomic<bool> m_uiDirty{true};       // UI 需要重绘（后台线程日志/状态更新时置位）
    ImFont *m_bigFont = nullptr;             // 大字号 Logo 字体
    ImFont *m_titleFont = nullptr;           // 卡片标题字体（加粗、略大）
    ImFont *m_monoFont = nullptr;            // 控制台等宽字体
    char m_gitUrl[256] = "https://github.com/chijichan/37AC.git";
    bool m_gitCloning = false;
    int m_winX = 100, m_winY = 50, m_winW = 1280, m_winH = 800;
    bool m_venvExists = false; // .venv 是否存在（缓存，避免每帧 _access 造成持续磁盘 IO）
    bool m_autoScroll = true;  // 控制台自动滚动到底部
};