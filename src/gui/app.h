#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gui/bridge.h"
#include "gui/components.h"
#include "gui/console.h"
#include "gui/lang.h"
#include "gui/theme.h"
#include "gui/tray.h"
#include "network/http_client.h"

// 一帧的跨线程状态快照（页面渲染只读此结构，避免直接读被 worker 写入的成员）
struct AppStateSnapshot
{
    std::string programDir;
    std::string projectRoot;
    std::string cliRoot;
    std::string serverRoot;

    bool pythonOk = false;
    std::string pythonVersion;
    bool venvExists = false;
    bool projectReady = false;
    bool downloading = false;

    bool procRunning = false;
    std::string procName;

    Bridge::Info model;
    bool modelLoading = false;

    std::map<std::string, std::string> envCli;
    std::map<std::string, std::string> envServer;
    std::map<std::string, std::string> envWeb;

    bool webOnline = false;
    bool tcpOnline = false;
    std::string webUrl;
    std::string frontendUrl;
};

// 页头右侧动作按钮
struct PageAction
{
    const char *id = "";
    const char *label = "";
    UI::Variant variant = UI::Variant::Ghost;
    UI::Icon icon = UI::Icon::None;
    std::function<void()> onClick;
    bool disabled = false;
};

// .env 取值（键不存在或为空时用默认值）
inline std::string EnvOr(const std::map<std::string, std::string> &env, const char *key,
                         const char *fallback)
{
    auto it = env.find(key);
    if (it != env.end() && !it->second.empty())
        return it->second;
    return fallback;
}

// 文件夹 / 文件选择对话框（返回空串表示取消）
std::string PickFolder(const char *title);
std::string PickFile(const char *filter, const char *title);

class App
{
public:
    enum class Page
    {
        Overview = 0,
        Model,
        Dataset,
        Node,
        Server,
        Env,
        Settings,
        Count
    };

    App();
    ~App();

    bool init(const char *title, int width, int height);
    void run();
    void renderFrame();

    // ---------------- 日志 ----------------
    void addLog(const std::string &msg, unsigned int = 0xFFFFFFFF); // 兼容旧调用
    void addInfo(const std::string &msg);
    void addSuccess(const std::string &msg);
    void addWarn(const std::string &msg);
    void addError(const std::string &msg);
    void notify(UI::Tone tone, const std::string &text);

    // ---------------- Python / 进程 ----------------
    bool runPython(const std::string &cmd, const std::string &cwd,
                   std::string &output, int &exitCode);
    bool checkPython();
    void createVenv(bool recreate);
    void startProcess(const std::string &name, const std::string &cmd,
                      const std::string &cwd);
    void stopProcess();
    bool isProcRunning() const { return m_procRunning.load(); }
    std::string procName();

    // ---------------- 业务动作 ----------------
    void startInitProject();
    void startGitDownload();
    void startTrain(bool resume);
    void startPredict(bool llm);
    void startNodeService();
    void startServer();
    void runBridge(const std::string &label, const std::string &args);
    void executeConsoleCommand(const std::string &cmd);

    // ---------------- 状态 ----------------
    AppStateSnapshot snapshot();
    void refreshModelInfo(); // 后台线程读取模型信息
    void refreshEnv();       // 读取三份 .env
    void markDirty();
    void goPage(Page p);

    // ---------------- 系统托盘 ----------------
    void requestTrayHide();
    void requestTrayShow();
    void requestTrayQuit();
    bool inTrayMode() const { return m_trayMode.load(); }
    // 窗口尺寸/位置回调：最小化时窗口尺寸会变成 0，需要缓存最后一次有效值
    void noteWindowSize(int w, int h);
    void noteWindowPos(int x, int y);
    void noteWindowMaximized(bool maximized);

    // ---------------- 路径 / 配置 ----------------
    std::string programDir();
    std::string projectRoot();
    std::string cliRoot();
    std::string serverRoot();
    std::string datasetDir();
    std::string croppedDir();
    std::string modelDir();
    std::string webHost();
    int webPort();
    int tcpPort();
    std::string backendUrl();
    std::string frontendUrl();
    bool writeEnvValue(const std::string &dir, const std::string &key, const std::string &value);

    // ---------------- 界面动作 ----------------
    void openUrl(const std::string &url);
    void openPath(const std::string &path);
    void openTextFile(const std::string &path);
    void copyToClipboard(const std::string &text);
    void askConfirm(const std::string &title, const std::string &body,
                    const std::string &okLabel, bool danger, std::function<void()> onOk);
    void askInput(const std::string &title, const std::string &label,
                  const std::string &value, bool password, const std::string &okLabel,
                  std::function<void(const std::string &)> onOk);

    // ---------------- 页面 ----------------
    void renderOverviewPage();
    void renderModelPage();
    void renderDatasetPage();
    void renderNodePage();
    void renderServerPage();
    void renderEnvPage();
    void renderSettingsPage();

private:
    // ---------------- 渲染 ----------------
    void render();
    void renderSidebar(const ImVec2 &display);
    void renderMain(const ImVec2 &display);
    void renderPageHeader();
    void renderPage();
    void renderConsolePanel(float height);
    void renderStatusBar(const ImVec2 &display);
    void renderModals();
    void renderConfirmModal();
    void renderInputModal();
    void renderTwoColumn(const std::function<void(float)> &left,
                         const std::function<void(float)> &right);
    bool navItem(const char *id, UI::Icon icon, const char *label, bool selected,
                 const char *badge = nullptr);
    void drawBrand();

    // ---------------- 线程 ----------------
    void procThreadFunc(const std::string &name, const std::string &cmd,
                        const std::string &cwd);
    void venvThreadFunc(bool recreate);
    void initProjectThreadFunc(const std::string &url);
    void infoThreadFunc();

    // ---------------- 界面上下文 / 托盘 ----------------
    bool createUi();
    void destroyUi();
    bool createWindow(int width, int height, bool visible);
    void enterTrayMode();
    void leaveTrayMode();
    void trimWorkingSet();

    void closeProcStdin();
    void setProcName(const std::string &name);
    void updateProjectPaths();
    void refreshVenvState();
    void setPythonState(bool ok, const std::string &ver);
    void probePorts(bool force);
    void syncModalPopup();

    // ---------------- 配置 ----------------
    void saveConfig();
    void loadConfig();

    // ---------------- 成员 ----------------
    std::string m_programDir;
    std::string m_projectRoot, m_cliRoot, m_serverRoot;

    Page m_page = Page::Overview;
    bool m_consoleOpen = true;
    bool m_consolePlain = false;
    bool m_autoScroll = true;
    float m_logRatio = 0.26f;
    int m_logRatioDrag = 0; // 0 空闲 / 1 拖动中
    int m_datasetChoice = 0;
    bool m_nodeNoUpdate = false;
    char m_repoBuf[128] = "";
    std::string m_hfToken;
    std::string m_pendingActionLabel;

    int m_winX = 100, m_winY = 50, m_winW = 1280, m_winH = 800;
    char m_gitUrl[512] = "https://github.com/chijichan/37AC/archive/refs/heads/master.zip";

    std::mutex m_stateMutex;
    bool m_pythonOk = false;
    std::string m_pythonVersion;
    bool m_venvExists = false;
    bool m_gitCloning = false;
    std::string m_procName;
    std::map<std::string, std::string> m_envCli, m_envServer, m_envWeb;

    std::atomic<bool> m_procRunning{false};
    std::atomic<unsigned long> m_procPid{0};
    std::thread m_procThread;
    std::mutex m_procStdinMutex;
    void *m_procStdinWrite = nullptr;

    std::mutex m_infoMutex;
    Bridge::Info m_modelInfo;
    std::atomic<bool> m_infoLoading{false};
    std::atomic<bool> m_infoRefreshRequested{false};
    std::thread m_infoThread;

    bool m_webOnline = false, m_tcpOnline = false;
    double m_lastProbe = -100.0;
    bool m_envDirty = true;

    std::atomic<bool> m_uiDirty{true};
    HttpClient m_http;
    ConsoleWidget m_console;
    Theme::Fonts m_fonts;

    // 系统托盘：隐藏时销毁 ImGui 上下文与字体缓存，只保留托盘消息窗口
    TrayIcon m_tray;
    std::atomic<bool> m_trayMode{false};
    std::atomic<bool> m_pendingTrayHide{false};
    std::atomic<bool> m_pendingTrayShow{false};
    std::atomic<bool> m_pendingTrayQuit{false};
    std::atomic<bool> m_quitRequested{false};
    bool m_trayAvailable = false;
    int m_trayWinX = 100, m_trayWinY = 50, m_trayWinW = 1280, m_trayWinH = 800;
    bool m_trayWinMaximized = false;

    // 跨线程 Toast（worker 线程不能直接碰 ImGui）
    std::thread::id m_mainThread;
    std::mutex m_toastMutex;
    std::vector<std::pair<int, std::string>> m_pendingToasts;

    // 模态框
    struct ConfirmState
    {
        bool active = false;
        bool popupOpened = false;
        std::string title, body, okLabel;
        bool danger = false;
        std::function<void()> onOk;
    } m_confirm;

    struct InputState
    {
        bool active = false;
        bool popupOpened = false;
        bool focusPending = true;
        std::string title, label, okLabel;
        bool password = false;
        char value[512] = "";
        std::function<void(const std::string &)> onOk;
    } m_input;
};
