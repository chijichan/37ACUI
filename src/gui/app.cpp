#include "app.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <direct.h>
#include <io.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

static GLFWwindow *g_window = nullptr;

static void glfw_error_callback(int error, const char *desc)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, desc);
}

// 过滤 ANSI 转义序列（如 \x1b[31m、\x1b[0m 等颜色标记）
static std::string stripAnsi(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[')
        {
            i += 2;
            while (i < s.size() && !((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z')))
                i++;
            continue; // 跳过 ESC[...m 等序列
        }
        out += s[i];
    }
    return out;
}

// ==================== 视觉辅助 ====================
// 状态色（绿/红/灰）
static const ImU32 g_colGreen = IM_COL32(34, 197, 94, 255);
static const ImU32 g_colRed = IM_COL32(239, 68, 68, 255);
static const ImU32 g_colGray = IM_COL32(150, 160, 172, 255);
static const ImU32 g_colAccent = IM_COL32(37, 99, 235, 255); // 品牌蓝，导航高亮竖线

// 状态圆点：在光标处画一个实心圆并预留空间
static void drawDot(ImU32 color)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float cy = p.y + ImGui::GetTextLineHeight() * 0.5f;
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + 5.0f, cy), 5.0f, color);
    ImGui::Dummy(ImVec2(14.0f, ImGui::GetTextLineHeight()));
    ImGui::SameLine();
}

static void drawStatusDot(bool ok) { drawDot(ok ? g_colGreen : g_colRed); }

// 标签（Tag）：圆角小标签，用于展示版本等状态信息
static void drawTag(const char *text, const ImVec4 &bg, const ImVec4 &fg)
{
    ImVec2 ts = ImGui::CalcTextSize(text);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetFrameHeight() - 4.0f;
    ImVec2 size(ts.x + 14.0f, h);
    ImU32 bgc = ImGui::ColorConvertFloat4ToU32(bg);
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), bgc, h * 0.5f);
    ImGui::SetCursorScreenPos(ImVec2(p.x + 7.0f, p.y + (size.y - ts.y) * 0.5f));
    ImGui::TextColored(fg, "%s", text);
    ImGui::SetCursorScreenPos(ImVec2(p.x + size.x, p.y));
    ImGui::Dummy(size);
    ImGui::SameLine();
}

// 导航项：选中时浅蓝背景 + 左侧亮色竖线；悬停时浅灰背景
static bool navItem(const char *label, bool selected)
{
    ImVec2 btnMin = ImGui::GetCursorScreenPos();
    ImVec2 btnMax = ImVec2(btnMin.x + ImGui::GetContentRegionAvail().x, btnMin.y + 36.0f);
    bool hovered = ImGui::IsMouseHoveringRect(btnMin, btnMax);

    if (selected || hovered)
    {
        ImGui::PushStyleColor(ImGuiCol_Button,
                              selected ? ImVec4(0.86f, 0.93f, 1.00f, 1.0f) : ImVec4(0.93f, 0.95f, 0.97f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.88f, 0.94f, 1.00f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.82f, 0.90f, 0.98f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.10f, 0.16f, 0.25f, 1.0f));
    }
    bool clicked = ImGui::Button(label, ImVec2(-1, 36));
    if (selected || hovered)
        ImGui::PopStyleColor(4);

    // 左侧亮色竖线（仅选中态）
    if (selected)
    {
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(btnMin.x + 3.0f, btnMin.y + 8.0f),
            ImVec2(btnMin.x + 6.0f, btnMax.y - 8.0f),
            g_colAccent, 1.5f);
    }
    return clicked;
}

// 退出导航项：悬停时浅红背景 + 红色竖线
static bool navExitItem(const char *label)
{
    ImVec2 btnMin = ImGui::GetCursorScreenPos();
    ImVec2 btnMax = ImVec2(btnMin.x + ImGui::GetContentRegionAvail().x, btnMin.y + 36.0f);
    bool hovered = ImGui::IsMouseHoveringRect(btnMin, btnMax);
    if (hovered)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.98f, 0.92f, 0.92f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.98f, 0.92f, 0.92f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.96f, 0.86f, 0.86f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.30f, 0.30f, 1.0f));
    }
    bool clicked = ImGui::Button(label, ImVec2(-1, 36));
    if (hovered)
        ImGui::PopStyleColor(4);
    if (hovered)
    {
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(btnMin.x + 3.0f, btnMin.y + 8.0f),
            ImVec2(btnMin.x + 6.0f, btnMax.y - 8.0f),
            IM_COL32(220, 80, 80, 255), 1.5f);
    }
    return clicked;
}

// ==================== 统一配置 ====================
// 所有持久化设置都写入 acui_settings.ini (acui.ini 由 ImGui 自动管理窗口布局)
static const char *CFG_FILE = "acui_settings.ini";

// 根据程序地址计算项目路径：m_projectRoot = programDir + project/37AC
void App::updateProjectPaths()
{
    std::string base = m_programDir;
    while (!base.empty() && (base.back() == '/' || base.back() == '\\'))
        base.pop_back();
    m_projectRoot = base + "/project/37AC";
    m_cliRoot = m_projectRoot + "/src/cli";
    m_serverRoot = m_projectRoot + "/src/server";
}

void App::saveConfig()
{
    if (g_window)
    {
        glfwGetWindowPos(g_window, &m_winX, &m_winY);
        glfwGetWindowSize(g_window, &m_winW, &m_winH);
    }
    FILE *f = fopen(CFG_FILE, "w");
    if (!f)
        return;
    fprintf(f, "win_x=%d\n", m_winX);
    fprintf(f, "win_y=%d\n", m_winY);
    fprintf(f, "win_w=%d\n", m_winW);
    fprintf(f, "win_h=%d\n", m_winH);
    fprintf(f, "lang=%d\n", (int)LangSys::I().lang());
    fprintf(f, "program=%s\n", m_programDir.c_str());
    fprintf(f, "project=%s\n", m_projectRoot.c_str());
    fprintf(f, "git_url=%s\n", m_gitUrl);
    fclose(f);
}

void App::loadConfig()
{
    FILE *f = fopen(CFG_FILE, "r");
    if (!f)
        return;
    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        int v;
        if (sscanf(line, "win_x=%d", &v) == 1)
            m_winX = v;
        else if (sscanf(line, "win_y=%d", &v) == 1)
            m_winY = v;
        else if (sscanf(line, "win_w=%d", &v) == 1)
            m_winW = v;
        else if (sscanf(line, "win_h=%d", &v) == 1)
            m_winH = v;
        else if (sscanf(line, "lang=%d", &v) == 1)
            LangSys::I().setLang((Lang)v);
        else if (strncmp(line, "program=", 8) == 0)
        {
            char *val = line + 8;
            size_t n = strlen(val);
            if (n > 0 && val[n - 1] == '\n')
                val[n - 1] = 0;
            m_programDir = val;
            updateProjectPaths();
        }
        else if (strncmp(line, "project=", 8) == 0 || strncmp(line, "git_dir=", 8) == 0)
        {
            // 兼容旧配置 git_dir=，统一读入 m_projectRoot
            char *val = line + strcspn(line, "=") + 1;
            size_t n = strlen(val);
            if (n > 0 && val[n - 1] == '\n')
                val[n - 1] = 0;
            m_projectRoot = val;
        }
        else if (strncmp(line, "git_url=", 8) == 0)
        {
            char *val = line + 8;
            size_t n = strlen(val);
            if (n > 0 && val[n - 1] == '\n')
                val[n - 1] = 0;
            strncpy_s(m_gitUrl, val, sizeof(m_gitUrl) - 1);
        }
    }
    fclose(f);
    // 若配置中没有 program=，用当前 exe 目录（init 中已自动获取）
    if (m_programDir.empty())
        updateProjectPaths();
    m_cliRoot = m_projectRoot + "/src/cli";
    m_serverRoot = m_projectRoot + "/src/server";
}

// ==================== 构造/析构 ====================
App::App() {}
App::~App()
{
    stopProcess();
    if (m_procThread.joinable())
        m_procThread.join();
}

// ==================== 初始化 ====================
bool App::init(const char *title, int width, int height)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!g_window)
        return false;

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);

    // 自动获取程序地址（exe 所在目录）
    {
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string p(exePath);
        auto pos = p.find_last_of("\\/");
        if (pos != std::string::npos)
            m_programDir = p.substr(0, pos);
        else
            m_programDir = p;
        updateProjectPaths();
    }

    // 初始化默认安全位置（主显示器居中）
    {
        int monCount = 0;
        GLFWmonitor **mons = glfwGetMonitors(&monCount);
        if (monCount > 0)
        {
            int wx, wy, ww, wh;
            glfwGetMonitorWorkarea(mons[0], &wx, &wy, &ww, &wh);
            m_winX = wx + (ww - width) / 2;
            m_winY = wy + (wh - height) / 2;
        }
    }

    // 从统一配置恢复（配置中如有 program= 会覆盖自动获取值）
    loadConfig();
    // 确保默认路径按程序地址推导
    updateProjectPaths();

    // 检查窗口位置是否有效，防止跑到屏幕外
    {
        bool onScreen = false;
        int monCount = 0;
        GLFWmonitor **mons = glfwGetMonitors(&monCount);
        for (int i = 0; i < monCount; i++)
        {
            int wx, wy, ww, wh;
            glfwGetMonitorWorkarea(mons[i], &wx, &wy, &ww, &wh);
            // 标题栏的任意一角在显示器区域内即认为有效
            if (m_winX >= wx && m_winX < wx + ww - 100 &&
                m_winY >= wy && m_winY < wy + wh - 30)
            {
                onScreen = true;
                break;
            }
        }
        if (!onScreen && monCount > 0)
        {
            int wx, wy;
            glfwGetMonitorWorkarea(mons[0], &wx, &wy, nullptr, nullptr);
            m_winX = wx + 100;
            m_winY = wy + 50;
        }
    }
    glfwSetWindowPos(g_window, m_winX, m_winY);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "acui.ini";

    ImGui::StyleColorsLight();
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding = 4.0f;
    s.FrameRounding = 3.0f;
    s.GrabRounding = 3.0f;
    // 37AC 配色方案（基于 PicoCSS light theme + 品牌色 #0f172a）
    s.Colors[ImGuiCol_WindowBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    s.Colors[ImGuiCol_ChildBg] = ImVec4(0.96f, 0.97f, 0.98f, 1.00f);
    s.Colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.09f, 0.16f, 1.00f); // #0f172a
    s.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.09f, 0.16f, 1.00f);
    s.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.96f, 0.97f, 0.98f, 1.00f);
    s.Colors[ImGuiCol_Tab] = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
    s.Colors[ImGuiCol_TabHovered] = ImVec4(0.85f, 0.87f, 0.90f, 1.00f);
    s.Colors[ImGuiCol_TabActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    s.Colors[ImGuiCol_Button] = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
    s.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.85f, 0.87f, 0.90f, 1.00f);
    s.Colors[ImGuiCol_ButtonActive] = ImVec4(0.75f, 0.78f, 0.82f, 1.00f);
    s.Colors[ImGuiCol_FrameBg] = ImVec4(0.96f, 0.97f, 0.98f, 1.00f);
    s.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
    s.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.85f, 0.87f, 0.90f, 1.00f);
    s.Colors[ImGuiCol_Header] = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
    s.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.85f, 0.87f, 0.90f, 1.00f);
    s.Colors[ImGuiCol_HeaderActive] = ImVec4(0.75f, 0.78f, 0.82f, 1.00f);
    s.Colors[ImGuiCol_Separator] = ImVec4(0.85f, 0.87f, 0.90f, 1.00f);
    s.Colors[ImGuiCol_Text] = ImVec4(0.06f, 0.09f, 0.16f, 1.00f); // #0f172a
    s.Colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.58f, 0.63f, 1.00f);
    s.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.75f, 0.78f, 0.82f, 1.00f);
    s.ScrollbarSize = 10.0f;
    s.ItemSpacing = ImVec2(8, 6);

    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 16.0f, nullptr,
                                 io.Fonts->GetGlyphRangesChineseFull());
    // 大字号 Logo 字体（26px ≈ 16px 的 1.6 倍）
    m_bigFont = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 26.0f, nullptr,
                                             io.Fonts->GetGlyphRangesChineseFull());
    // 卡片标题字体（加粗，比正文大 2px）
    m_titleFont = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyhbd.ttc", 18.0f, nullptr,
                                               io.Fonts->GetGlyphRangesChineseFull());
    // 控制台等宽字体（Consolas），缺失的 CJK 字形用微软雅黑补齐
    m_monoFont = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/consola.ttf", 15.0f, nullptr,
                                              io.Fonts->GetGlyphRangesChineseFull());
    if (m_monoFont)
    {
        ImFontConfig cfg;
        cfg.MergeMode = true; // 合并到最后一个字体（consola）
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 15.0f, &cfg,
                                     io.Fonts->GetGlyphRangesChineseFull());
    }

    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // 从 acui_settings.ini 恢复了 win pos，但创建窗口用了默认大小，需要调整
    // 创建时的 width/height 来自第一启动默认值，配置在 loadConfig 后生效
    // 下次启动窗口位置由 glfwSetWindowPos 恢复

    // 设置子进程环境变量：UTF-8 编码 + 无缓冲输出
    // PYTHONUTF8=1 启用 UTF-8 模式，避免 pip 用 GBK 解码含中文注释的 requirements.txt 报错
    _putenv_s("PYTHONIOENCODING", "utf-8");
    _putenv_s("PYTHONUNBUFFERED", "1");
    _putenv_s("PYTHONUTF8", "1");

    checkPython();
    addInfo("[OK] 37ACUI started");
    addInfo(std::string("project: ") + m_projectRoot);
    if (m_pythonOk)
        addInfo(std::string("Python: ") + m_pythonVersion);
    else
        addWarn(std::string("Python not found"));

    return true;
}

// ==================== 日志 ====================
void App::addLog(const std::string &msg, unsigned int)
{
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    struct tm tm;
    localtime_s(&tm, &tt);
    char buf[32];
    strftime(buf, sizeof(buf), "[%H:%M:%S]", &tm);
    m_console.AddLog(std::string(buf) + " " + msg);
}

// ==================== Python ====================
bool App::checkPython()
{
    std::string out;
    int ec = 0;
    bool ok = runPython("--version", m_projectRoot, out, ec);
    if (ok && ec == 0)
    {
        m_pythonVersion = out;
        m_pythonOk = true;
        std::string v;
        int vc = 0;
        runPython("-c \"import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}')\"",
                  m_projectRoot, v, vc);
        if (vc == 0 && !v.empty())
            m_pythonVersion = v;
    }
    else
    {
        m_pythonVersion = "not found";
        m_pythonOk = false;
    }
    return m_pythonOk;
}

bool App::runPython(const std::string &cmd, const std::string &cwd,
                    std::string &output, int &exitCode)
{
    output.clear();
    exitCode = -1;
    std::string py = cwd + "/.venv/Scripts/python.exe";
    if (_access(py.c_str(), 0) != 0)
    {
        py = cwd + "/../.venv/Scripts/python.exe";
        if (_access(py.c_str(), 0) != 0)
        {
            py = cwd + "/../../.venv/Scripts/python.exe";
            if (_access(py.c_str(), 0) != 0)
                py = "python";
        }
    }
    // 用 cmd /c + cd /d 在目标目录运行，确保 .venv 等相对路径落在正确位置
    std::string fc = "cmd /c chcp 65001>nul & cd /d \"" + cwd + "\" && ";
    if (py != "python")
        fc += "\"" + py + "\"";
    else
        fc += "python";
    fc += " -u " + cmd + " 2>&1";
    FILE *pipe = _popen(fc.c_str(), "r");
    if (!pipe)
    {
        output = "cannot start";
        return false;
    }
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;
    exitCode = _pclose(pipe);
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
        output.pop_back();
    return true;
}

// ==================== 虚拟环境 ====================
// 在后台线程中执行删除+创建，避免阻塞主渲染线程导致界面卡死
void App::createVenv(bool recreate)
{
    if (m_procRunning)
    {
        addWarn("[warn] process already running");
        return;
    }
    m_procRunning = true;
    if (m_procThread.joinable())
        m_procThread.join();
    m_procThread = std::thread(&App::venvThreadFunc, this, recreate);
}

void App::venvThreadFunc(bool recreate)
{
    std::string venvDir = m_projectRoot + "/.venv";
    if (recreate && _access(venvDir.c_str(), 0) == 0)
    {
        addWarn(std::string(TR("env.venv_del_ok")));
        system(("cmd /c rmdir /s /q \"" + venvDir + "\"").c_str());
    }
    else if (!recreate && _access(venvDir.c_str(), 0) == 0)
    {
        addWarn(std::string(TR("env.venv_exist")));
    }

    std::string o;
    int c = 0;
    runPython("-m venv .venv", m_projectRoot, o, c);
    if (c == 0)
    {
        addSuccess(TR("env.venv_ok"));
        checkPython(); // 创建后立即刷新，识别刚生成的 venv 解释器
    }
    else
    {
        addError(TR("env.venv_fail"));
        if (!o.empty())
            addError("[venv] " + o);
        // 针对性提示：解析常见错误，给出可操作的解决办法
        if (o.find("Permission denied") != std::string::npos)
            addError(std::string(TR("env.venv_perm")));
    }
    m_procRunning = false;
}

// ==================== 异步进程 ====================
void App::procThreadFunc(const std::string &name, const std::string &cmd,
                         const std::string &cwd)
{
    addInfo(std::string("[start] ") + name + ": " + cmd);

    std::string py = cwd + "/.venv/Scripts/python.exe";
    if (_access(py.c_str(), 0) != 0)
    {
        py = cwd + "/../.venv/Scripts/python.exe";
        if (_access(py.c_str(), 0) != 0)
        {
            py = cwd + "/../../.venv/Scripts/python.exe";
            if (_access(py.c_str(), 0) != 0)
                py = "python";
        }
    }
    std::string fc = "\"" + py + "\" -u " + cmd;

    HANDLE hOutR, hOutW;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), 0, TRUE};
    CreatePipe(&hOutR, &hOutW, &sa, 0);
    SetHandleInformation(hOutR, HANDLE_FLAG_INHERIT, 0);
    HANDLE hInR, hInW;
    CreatePipe(&hInR, &hInW, &sa, 0);
    SetHandleInformation(hInW, HANDLE_FLAG_INHERIT, 0);
    m_procStdinWrite = hInW;

    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hOutW;
    si.hStdError = hOutW;
    si.hStdInput = hInR;
    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessA(nullptr, &fc[0], 0, 0, TRUE, CREATE_NO_WINDOW, 0, cwd.c_str(), &si, &pi);
    CloseHandle(hOutW);
    CloseHandle(hInR);

    if (!ok)
    {
        addError(std::string("[error] ") + name + " code=" + std::to_string(GetLastError()));
        CloseHandle(hOutR);
        m_procRunning = false;
        m_procStdinWrite = nullptr;
        return;
    }
    CloseHandle(pi.hThread);

    char buf[4096];
    DWORD read, avail;
    std::string part;
    while (m_procRunning)
    {
        if (PeekNamedPipe(hOutR, 0, 0, 0, &avail, 0))
        {
            if (avail > 0)
            {
                if (ReadFile(hOutR, buf, std::min<DWORD>((DWORD)sizeof(buf) - 1, avail), &read, 0) && read > 0)
                {
                    buf[read] = 0;
                    part += buf;
                    size_t p;
                    while ((p = part.find('\n')) != std::string::npos)
                    {
                        std::string l = part.substr(0, p);
                        while (!l.empty() && l.back() == '\r')
                            l.pop_back();
                        if (!l.empty())
                            addInfo("  " + stripAnsi(l));
                        part.erase(0, p + 1);
                    }
                }
                else
                    break; // ReadFile failed, process ended
            }
            else
            {
                // No data yet — check if process is still alive
                DWORD ec2 = 0;
                if (!GetExitCodeProcess(pi.hProcess, &ec2) || ec2 != STILL_ACTIVE)
                    break; // process exited
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        else
            break; // PeekNamedPipe failed
    }
    while (!part.empty() && (part.back() == '\n' || part.back() == '\r'))
        part.pop_back();
    if (!part.empty())
        addInfo("  " + stripAnsi(part));

    DWORD ec = 0;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess);
    CloseHandle(hOutR);
    if (ec == 0)
        addSuccess(std::string("[done] ") + name + " exit=" + std::to_string(ec));
    else
        addError(std::string("[done] ") + name + " exit=" + std::to_string(ec));
    m_procRunning = false;
    m_procStdinWrite = nullptr;
}

void App::startProcess(const std::string &name, const std::string &cmd,
                       const std::string &cwd)
{
    if (m_procRunning)
    {
        addWarn("[warn] process already running");
        return;
    }
    if (m_procStdinWrite)
    {
        CloseHandle(m_procStdinWrite);
        m_procStdinWrite = nullptr;
    }
    m_procRunning = true;
    if (m_procThread.joinable())
        m_procThread.join();
    m_procThread = std::thread(&App::procThreadFunc, this, name, cmd, cwd);
}

void App::stopProcess()
{
    if (!m_procRunning)
        return;
    addWarn("[warn] terminating...");
    m_procRunning = false;
    if (m_procStdinWrite)
    {
        CloseHandle(m_procStdinWrite);
        m_procStdinWrite = nullptr;
    }
    system("taskkill /f /im python.exe >nul 2>&1");
}

// ==================== Git ====================
void App::startGitClone(const std::string &url, const std::string &dir)
{
    if (m_procRunning)
    {
        addWarn("[warn] process running");
        return;
    }
    addInfo(std::string("[Git] cloning: ") + url);
    m_gitCloning = true;
    m_procRunning = true;
    if (m_procThread.joinable())
        m_procThread.join();
    m_procThread = std::thread([this, url, dir]()
                               {
        FILE *t = _popen("git --version 2>&1","r");
        if (t) { char b[256]; if(fgets(b,sizeof(b),t)){std::string v(b); while(!v.empty()&&(v.back()=='\n'||v.back()=='\r'))v.pop_back(); addInfo(std::string("[Git] ")+v);} _pclose(t); }
        else { addError("[Git] git not found"); m_procRunning=m_gitCloning=false; return; }
        if (_access(dir.c_str(),0)==0) { addWarn("[Git] removing old..."); system(("cmd /c rmdir /s /q \""+dir+"\"").c_str()); }
        std::string cmd = "git clone --depth 1 \""+url+"\" \""+dir+"\" 2>&1";
        FILE *pipe = _popen(cmd.c_str(),"r");
        if(!pipe){addError("[Git] cannot start"); m_procRunning=m_gitCloning=false; return;}
        char buf[4096];
        while(fgets(buf,sizeof(buf),pipe)){std::string l(buf); while(!l.empty()&&(l.back()=='\n'||l.back()=='\r'))l.pop_back(); if(!l.empty())addInfo("  "+stripAnsi(l));}
        int ec=_pclose(pipe);
        if(ec==0){addSuccess("[Git] clone success!"); m_projectRoot=dir; m_cliRoot=dir+"/src/cli"; m_serverRoot=dir+"/src/server"; checkPython();}
        else addError(std::string("[Git] clone failed exit=")+std::to_string(ec));
        m_procRunning=m_gitCloning=false; });
}

// ==================== 主循环 ====================
void App::run()
{
    while (!glfwWindowShouldClose(g_window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        render();
        ImGui::Render();
        int dw, dh;
        glfwGetFramebufferSize(g_window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(1.00f, 1.00f, 1.00f, 1.00f); // 白色背景（与浅色主题一致）
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(g_window);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 窗口关闭前保存配置（此时 g_window 仍然有效）
    saveConfig();

    stopProcess();
    if (m_procThread.joinable())
        m_procThread.join();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(g_window);
    g_window = nullptr;
    glfwTerminate();
}

// ==================== 渲染 ====================
void App::render()
{
    ImVec2 vs = ImGui::GetIO().DisplaySize;
    float statusH = 24.0f; // 与状态栏实际高度一致，避免底部露出缝隙
    float logH = vs.y * 0.4f;
    float navW = vs.x * 0.25f; // 左侧占 1/4
    float contentX = navW;
    float contentW = vs.x - navW;
    float contentH = vs.y - statusH - logH;

    // ---- 左侧导航栏 ----
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(navW, vs.y - statusH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.965f, 0.975f, 0.985f, 1.0f)); // 侧边栏浅色背景
    ImGui::Begin("SideNav", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    renderSideNav();
    ImGui::End();

    // ---- 右侧内容区 ----
    ImGui::SetNextWindowPos(ImVec2(contentX, 0));
    ImGui::SetNextWindowSize(ImVec2(contentW, contentH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::Begin("Content", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(3);
    renderContentArea();
    ImGui::End();

    // ---- 右侧控制台区 ----
    ImGui::SetNextWindowPos(ImVec2(contentX, contentH));
    ImGui::SetNextWindowSize(ImVec2(contentW, logH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
    ImGui::Begin("Log", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(3);
    renderLogArea();
    ImGui::End();

    renderStatusBar();
    if (m_showDemo)
        ImGui::ShowDemoWindow(&m_showDemo);
}

// ==================== 左侧导航栏 ====================
void App::renderSideNav()
{
    // 计算导航内容总高度，实现垂直居中
    float contentH = ImGui::GetWindowHeight();
    float logoH = (m_bigFont ? m_bigFont->FontSize : ImGui::GetTextLineHeightWithSpacing()) + 16.0f;
    float sepH = 8;
    float navH = 36.0f * 3 + ImGui::GetStyle().ItemSpacing.y * 3; // 3 个按钮
    float bottomSpacing = 20;
    float exitH = 36.0f;
    float total = logoH + sepH + navH + bottomSpacing + sepH + exitH;

    // 顶部留白（垂直居中）
    float topPad = (contentH - total) / 2.0f;
    if (topPad > 0)
        ImGui::Dummy(ImVec2(0, topPad));

    // Logo（大字号字体 + 左右居中）
    if (m_bigFont)
        ImGui::PushFont(m_bigFont);
    float logoW = ImGui::CalcTextSize("37ACUI").x;
    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((availW - logoW) / 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.25f, 0.45f, 0.65f, 1.0f));
    ImGui::Text("37ACUI");
    ImGui::PopStyleColor();
    if (m_bigFont)
        ImGui::PopFont();
    ImGui::Spacing();
    ImGui::Spacing();

    // 导航按钮（选中：浅蓝背景 + 左侧亮色竖线）
    const char *navItems[] = {TR("nav.launch"), TR("nav.tools"), TR("nav.settings")};
    for (int i = 0; i < 3; i++)
    {
        if (navItem(navItems[i], m_activeNav == i))
            m_activeNav = i;
        ImGui::Spacing();
    }

    // 底部：退出按钮
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    if (navExitItem(TR("nav.exit")))
        glfwSetWindowShouldClose(g_window, true);
}

// ==================== 右侧内容区 ====================
void App::renderContentArea()
{
    if (m_activeNav == 0)
        renderLaunchPanel();
    else if (m_activeNav == 1)
        renderToolsPanel();
    else
        renderSettingsPanel();
}

// ==================== 卡片容器 ====================
void App::beginCard(const char *id, const char *title, float width)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.985f, 0.988f, 0.993f, 1.0f)); // 卡片浅色背景
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.87f, 0.90f, 0.93f, 1.0f));     // 卡片边框
    ImGui::BeginChild(id, ImVec2(width, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize,
                      ImGuiWindowFlags_NoScrollbar);

    // 标题：加粗 + 略大（视觉层级）
    if (m_titleFont)
        ImGui::PushFont(m_titleFont);
    ImGui::TextUnformatted(title);
    if (m_titleFont)
        ImGui::PopFont();

    // 标题下浅色分隔线
    ImGui::Spacing();
    ImVec2 sepMin = ImGui::GetCursorScreenPos();
    ImVec2 sepMax(sepMin.x + ImGui::GetContentRegionAvail().x, sepMin.y + 1.0f);
    ImGui::GetWindowDrawList()->AddRectFilled(sepMin, sepMax, IM_COL32(228, 232, 238, 255));
    ImGui::Dummy(ImVec2(0, 6.0f));
    ImGui::Spacing();
}

void App::endCard()
{
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ImGui::Spacing();
}

// ==================== 启动页（环境+Git+服务端+CLI） ====================
void App::renderLaunchPanel()
{
    if (ImGui::BeginChild("LaunchScroll", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar))
    {
        renderEnvPanel(); // 环境 + Git 整合
        renderServerPanel();
        renderCliPanel();
    }
    ImGui::EndChild();
}

// ==================== 工具页 ====================
void App::renderToolsPanel()
{
    beginCard("card_tools", TR("menu.tools"));
    {
        if (ImGui::Checkbox("ImGui Demo", &m_showDemo))
        {
            // 切换 demo 窗口
        }
        ImGui::Spacing();
        if (ImGui::Button(TR("menu.refresh"), ImVec2(180, 36)))
            checkPython();
    }
    endCard();
}

// ==================== 设置页（语言 + Git 配置） ====================
void App::renderSettingsPanel()
{
    // ---- 语言 ----
    beginCard("card_lang", TR("settings.language"));
    {
        bool en = (LangSys::I().lang() == Lang::English);
        bool cn = (LangSys::I().lang() == Lang::Chinese);
        if (ImGui::RadioButton(TR("settings.english"), en))
            LangSys::I().setLang(Lang::English);
        ImGui::Spacing();
        if (ImGui::RadioButton(TR("settings.chinese"), cn))
            LangSys::I().setLang(Lang::Chinese);
    }
    endCard();

    // ---- 程序地址 ----
    beginCard("card_prog", TR("settings.program_dir"));
    {
        float bw = 80.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - bw - ImGui::GetStyle().ItemSpacing.x);
        static char programBuf[512] = "";
        strncpy_s(programBuf, m_programDir.c_str(), sizeof(programBuf) - 1);
        if (ImGui::InputText("##pd", programBuf, sizeof(programBuf)))
        {
            m_programDir = programBuf;
            updateProjectPaths();
        }
        ImGui::SameLine();
        ImGui::PushID("browse_program");
        if (ImGui::Button(TR("git.browse"), ImVec2(bw, 0)))
        {
            char path[MAX_PATH] = {};
            BROWSEINFOA bi = {};
            bi.lpszTitle = "Select program directory";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
            if (pidl)
            {
                if (SHGetPathFromIDListA(pidl, path))
                {
                    m_programDir = path;
                    updateProjectPaths();
                }
                CoTaskMemFree(pidl);
            }
        }
        ImGui::PopID();
        ImGui::TextDisabled("  %s %s", TR("git.dir"), m_projectRoot.c_str());
    }
    endCard();

    // ---- Git 下载配置 ----
    beginCard("card_gitset", TR("git.title"));
    {
        ImGui::Text("%s", TR("git.url"));
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##gu", m_gitUrl, sizeof(m_gitUrl));

        // 目标目录直接使用 m_projectRoot（std::string 需要缓冲）
        ImGui::Text("%s", TR("git.dir"));
        float bw = 80.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - bw - ImGui::GetStyle().ItemSpacing.x);
        static char projectBuf[512] = "";
        strncpy_s(projectBuf, m_projectRoot.c_str(), sizeof(projectBuf) - 1);
        if (ImGui::InputText("##gd", projectBuf, sizeof(projectBuf)))
        {
            m_projectRoot = projectBuf;
            m_cliRoot = m_projectRoot + "/src/cli";
            m_serverRoot = m_projectRoot + "/src/server";
        }
        ImGui::SameLine();
        ImGui::PushID("browse_project");
        if (ImGui::Button(TR("git.browse"), ImVec2(bw, 0)))
        {
            char path[MAX_PATH] = {};
            BROWSEINFOA bi = {};
            bi.lpszTitle = "Select target directory";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
            if (pidl)
            {
                if (SHGetPathFromIDListA(pidl, path))
                    m_projectRoot = path;
                CoTaskMemFree(pidl);
            }
            // 目标目录 = 项目根
            m_cliRoot = m_projectRoot + "/src/cli";
            m_serverRoot = m_projectRoot + "/src/server";
        }
        ImGui::PopID();
    }
    endCard();
}

// ==================== 控制台 ====================
void App::renderLogArea()
{
    float ih = 28.0f; // 始终保留输入框高度
    float th = ImGui::GetFrameHeightWithSpacing() + 4;
    // 标题（加粗，视觉层级）
    if (m_titleFont)
        ImGui::PushFont(m_titleFont);
    ImGui::TextColored(ImVec4(0.15f, 0.30f, 0.45f, 1.0f), "%s", TR("console.title"));
    if (m_titleFont)
        ImGui::PopFont();
    ImGui::SameLine();
    if (ImGui::SmallButton(TR("console.clear")))
        m_console.ClearLog();
    ImGui::SameLine();
    ImGui::Text("  %s", TR("console.hint"));
    ImGui::Separator();
    auto send = [this](const std::string &c)
    {
        if (m_procStdinWrite)
        {
            std::string s = c + "\n";
            DWORD w;
            WriteFile(m_procStdinWrite, s.c_str(), (DWORD)s.size(), &w, 0);
        }
        else
        {
            // 如果没有进程在运行，把输入当作命令来启动进程
            // 支持: train / resume / predict / node / verify / menu
            std::string cmd;
            if (c == "train")
                cmd = "main.py --mode 1";
            else if (c == "resume")
                cmd = "main.py --mode 1 --resume auto";
            else if (c == "predict")
                cmd = "main.py --mode 2";
            else if (c == "node")
                cmd = "main.py --mode 4";
            else if (c == "verify")
                cmd = "main.py --mode 3";
            else if (c == "menu")
                cmd = "main.py";
            else if (c == "server")
                cmd = "runserver.py";
            else
            {
                addWarn(std::string("[warn] no process running, type: train/resume/predict/node/verify/menu/server"));
                return;
            }
            startProcess("console: " + c, cmd, m_cliRoot);
        }
    };
    m_console.Draw("##con", ImVec2(0, -ih - th), true, send,
                   "train | resume | predict | node | verify | menu | server", m_monoFont);
}

// ==================== 状态栏 ====================
void App::renderStatusBar()
{
    ImVec2 vs = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0, vs.y - 24));
    ImGui::SetNextWindowSize(ImVec2(vs.x, 24));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("SB", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(3);

    // Python 状态：绿/红圆点 + 文字
    drawDot(m_pythonOk ? g_colGreen : g_colRed);
    if (m_pythonOk)
        ImGui::TextColored(ImVec4(0.11f, 0.41f, 0.33f, 1), "%s", TR("status.python_ok"));
    else
        ImGui::TextColored(ImVec4(0.72f, 0.41f, 0.42f, 1), "%s", TR("status.python_no"));
    ImGui::SameLine();
    ImGui::Text("%s %s", TR("status.project"), m_projectRoot.c_str());

    // 进程状态显示在右下角
    ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetCursorPosX() - 100.0f);
    if (m_procRunning)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1), "%s%s", TR("nav.status"), TR("nav.running"));
        ImGui::SameLine();
        if (ImGui::SmallButton(TR("nav.stop")))
            stopProcess();
    }
    else
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "%s%s", TR("nav.status"), TR("nav.idle"));
    }

    ImGui::End();
}

// ==================== 环境面板（含 Git 下载） ====================
void App::renderEnvPanel()
{
    float availW = ImGui::GetContentRegionAvail().x;
    float gap = ImGui::GetStyle().ItemSpacing.x;
    float colW = (availW - gap) / 2.0f;
    if (colW < 240.0f)
        colW = availW; // 窗口过窄时退化为单列

    // 左列：Python 状态 + 路径
    ImGui::BeginGroup();
    {
        beginCard("card_py", TR("env.python"), colW);
        {
            // 状态行：绿/红圆点 + 状态文字 + 版本标签 + 重新检测
            drawStatusDot(m_pythonOk);
            ImGui::TextUnformatted(m_pythonOk ? TR("env.ready") : TR("env.nofound"));
            if (m_pythonOk)
            {
                ImGui::SameLine();
                drawTag((std::string(TR("env.version")) + " " + m_pythonVersion).c_str(),
                        ImVec4(0.89f, 0.94f, 1.00f, 1.0f), ImVec4(0.10f, 0.32f, 0.58f, 1.0f));
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(TR("env.recheck")))
                checkPython();
        }
        endCard();

        beginCard("card_paths", TR("env.paths"), colW);
        {
            ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.63f, 1.0f), "%s", TR("env.root"));
            ImGui::SameLine();
            ImGui::TextWrapped("%s", m_projectRoot.c_str());
            ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.63f, 1.0f), "%s", TR("env.cli"));
            ImGui::SameLine();
            ImGui::TextWrapped("%s", m_cliRoot.c_str());
            ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.63f, 1.0f), "%s", TR("env.srv"));
            ImGui::SameLine();
            ImGui::TextWrapped("%s", m_serverRoot.c_str());
        }
        endCard();
    }
    ImGui::EndGroup();

    ImGui::SameLine();

    // 右列：快捷操作 + GitHub 下载
    ImGui::BeginGroup();
    {
        beginCard("card_actions", TR("env.actions"), colW);
        {
            float bw = (ImGui::GetContentRegionAvail().x - gap) / 2.0f;
            if (ImGui::Button(TR("env.install_cli"), ImVec2(bw, 34)))
                startProcess("install CLI", "-m pip install -r requirements.txt", m_cliRoot);
            ImGui::SameLine();
            if (ImGui::Button(TR("env.install_srv"), ImVec2(bw, 34)))
                startProcess("install server", "-m pip install -r requirements.txt", m_serverRoot);
            if (ImGui::Button(TR("env.verify"), ImVec2(-1, 34)))
                startProcess("verify env", "verify_env.py", m_projectRoot);
            ImGui::Spacing();
            if (ImGui::Button(TR("env.venv"), ImVec2(-1, 34)))
                createVenv(false);
            // .venv 已存在时提供“删除并重建”，避免 Permission denied
            if (_access((m_projectRoot + "/.venv").c_str(), 0) == 0)
            {
                if (ImGui::Button(TR("env.venv_del"), ImVec2(-1, 34)))
                    createVenv(true);
            }
        }
        endCard();

        beginCard("card_git", TR("git.title"), colW);
        {
            ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.63f, 1.0f), "%s", TR("git.url"));
            ImGui::TextWrapped("%s", m_gitUrl);
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.63f, 1.0f), "%s", TR("git.dir"));
            ImGui::TextWrapped("%s", m_projectRoot.c_str());
            ImGui::Spacing();
            if (m_gitCloning)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "%s", TR("git.cloning"));
            }
            else
            {
                if (ImGui::Button(TR("git.clone"), ImVec2(-1, 36)))
                {
                    std::string u(m_gitUrl), d(m_projectRoot);
                    if (!u.empty() && !d.empty())
                        startGitClone(u, d);
                    else
                        addError(std::string(TR("git.fill")));
                }
                if (ImGui::Button(TR("git.check"), ImVec2(-1, 36)))
                {
                    if (_access(m_projectRoot.c_str(), 0) == 0)
                    {
                        addSuccess(std::string(TR("git.exists")));
                        m_cliRoot = m_projectRoot + "/src/cli";
                        m_serverRoot = m_projectRoot + "/src/server";
                        checkPython();
                    }
                    else
                        addWarn(std::string(TR("git.none")));
                }
            }
        }
        endCard();
    }
    ImGui::EndGroup();
}

// ==================== 服务端面板 ====================
void App::renderServerPanel()
{
    beginCard("card_srv", TR("srv.title"));
    {
        // 状态行：圆点 + 状态文字
        drawDot(m_serverStatus == 1 ? g_colGreen : (m_serverStatus == 0 ? g_colRed : g_colGray));
        if (m_serverStatus == 1)
            ImGui::TextColored(ImVec4(0.11f, 0.41f, 0.33f, 1.0f), "%s", TR("srv.on"));
        else if (m_serverStatus == 0)
            ImGui::TextColored(ImVec4(0.72f, 0.41f, 0.42f, 1.0f), "%s", TR("srv.off"));
        else
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", TR("srv.unknown"));
        ImGui::Spacing();

        if (ImGui::Button(TR("srv.start"), ImVec2(-1, 40)))
            startProcess("start server", "runserver.py", m_serverRoot);
        float bw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
        if (ImGui::Button(TR("srv.check"), ImVec2(bw, 36)))
        {
            m_serverStatus = m_http.ping() ? 1 : 0;
            if (m_serverStatus == 1)
                addSuccess(TR("srv.online"));
            else
                addWarn(TR("srv.offline"));
        }
        ImGui::SameLine();
        if (ImGui::Button(TR("srv.api"), ImVec2(bw, 36)))
            ShellExecuteA(0, "open", "http://127.0.0.1:13138/upload", 0, 0, SW_SHOW);
    }
    endCard();
}

// ==================== CLI 面板 ====================
void App::renderCliPanel()
{
    float gap = ImGui::GetStyle().ItemSpacing.x;

    beginCard("card_cli_train", TR("cli.train"));
    {
        float bw = (ImGui::GetContentRegionAvail().x - gap) / 2.0f;
        if (ImGui::Button(TR("cli.btn_train"), ImVec2(bw, 40)))
            startProcess("train", "main.py --mode 1", m_cliRoot);
        ImGui::SameLine();
        if (ImGui::Button(TR("cli.btn_resume"), ImVec2(bw, 40)))
            startProcess("resume", "main.py --mode 1 --resume auto", m_cliRoot);
        if (ImGui::Button(TR("cli.btn_predict"), ImVec2(-1, 40)))
            startProcess("predict", "main.py --mode 2", m_cliRoot);
    }
    endCard();

    beginCard("card_cli_node", TR("cli.node"));
    {
        float bw = (ImGui::GetContentRegionAvail().x - gap) / 2.0f;
        if (ImGui::Button(TR("cli.btn_node"), ImVec2(bw, 40)))
            startProcess("node", "main.py --mode 4", m_cliRoot);
        ImGui::SameLine();
        if (ImGui::Button(TR("cli.btn_verify"), ImVec2(bw, 40)))
            startProcess("verify", "main.py --mode 3", m_cliRoot);
        if (ImGui::Button(TR("cli.btn_menu"), ImVec2(-1, 40)))
            startProcess("menu", "main.py", m_cliRoot);
    }
    endCard();
}