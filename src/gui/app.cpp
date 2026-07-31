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

// ==================== 统一配置 ====================
// 所有持久化设置都写入 acui_settings.ini (acui.ini 由 ImGui 自动管理窗口布局)
static const char *CFG_FILE = "acui_settings.ini";

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
    fprintf(f, "project=%s\n", m_projectRoot.c_str());
    fprintf(f, "git_url=%s\n", m_gitUrl);
    fprintf(f, "git_dir=%s\n", m_gitDir);
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
        else if (sscanf(line, "project=%511s", m_gitDir) == 1)
            m_projectRoot = m_gitDir;
        else if (strncmp(line, "git_url=", 8) == 0)
        {
            char *val = line + 8;
            size_t n = strlen(val);
            if (n > 0 && val[n - 1] == '\n')
                val[n - 1] = 0;
            strncpy_s(m_gitUrl, val, sizeof(m_gitUrl) - 1);
        }
        else if (strncmp(line, "git_dir=", 8) == 0)
        {
            char *val = line + 8;
            size_t n = strlen(val);
            if (n > 0 && val[n - 1] == '\n')
                val[n - 1] = 0;
            strncpy_s(m_gitDir, val, sizeof(m_gitDir) - 1);
        }
    }
    fclose(f);
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

    // 从统一配置恢复
    loadConfig();

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

    ImGui::StyleColorsDark();
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding = 4.0f;
    s.FrameRounding = 3.0f;
    s.GrabRounding = 3.0f;
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    s.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    s.Colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    s.Colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.28f, 1.00f);
    s.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    s.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.35f, 1.00f);
    s.Colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.22f, 0.30f, 1.00f);
    s.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    s.Colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    s.ScrollbarSize = 10.0f;
    s.ItemSpacing = ImVec2(8, 6);

    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 16.0f, nullptr,
                                 io.Fonts->GetGlyphRangesChineseFull());

    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // 从 acui_settings.ini 恢复了 win pos，但创建窗口用了默认大小，需要调整
    // 创建时的 width/height 来自第一启动默认值，配置在 loadConfig 后生效
    // 下次启动窗口位置由 glfwSetWindowPos 恢复

    // 设置子进程环境变量：UTF-8 编码 + 无缓冲输出
    _putenv_s("PYTHONIOENCODING", "utf-8");
    _putenv_s("PYTHONUNBUFFERED", "1");

    // 确保项目根路径正确（默认 E:/pj/37AC，如果配置读出来是 UI 自己的路径则修正）
    if (m_projectRoot.empty() ||
        m_projectRoot.find("37ACUI") != std::string::npos ||
        m_projectRoot.find("37AC") == std::string::npos)
    {
        m_projectRoot = "E:/pj/37AC";
        m_cliRoot = m_projectRoot + "/src/cli";
        m_serverRoot = m_projectRoot + "/src/server";
    }

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
    std::string py = cwd + "/../.venv/Scripts/python.exe";
    if (_access(py.c_str(), 0) != 0)
    {
        py = cwd + "/../../.venv/Scripts/python.exe";
        if (_access(py.c_str(), 0) != 0)
            py = "python";
    }
    std::string fc = "\"" + py + "\" -u " + cmd + " 2>&1";
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

// ==================== 异步进程 ====================
void App::procThreadFunc(const std::string &name, const std::string &cmd,
                         const std::string &cwd)
{
    addInfo(std::string("[start] ") + name + ": " + cmd);

    std::string py = cwd + "/../.venv/Scripts/python.exe";
    if (_access(py.c_str(), 0) != 0)
    {
        py = cwd + "/../../.venv/Scripts/python.exe";
        if (_access(py.c_str(), 0) != 0)
            py = "python";
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
        glClearColor(0.06f, 0.06f, 0.08f, 1.00f);
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
    float statusH = 26.0f;
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
    ImGui::Begin("SideNav", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);
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
    // Logo
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1), "37ACUI");
    ImGui::Separator();
    ImGui::Spacing();

    // 导航按钮
    const char *navItems[] = {TR("nav.launch"), TR("nav.tools"), TR("nav.settings")};
    for (int i = 0; i < 3; i++)
    {
        bool selected = (m_activeNav == i);
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.65f, 1.0f));
        if (ImGui::Button(navItems[i], ImVec2(-1, 36)))
            m_activeNav = i;
        if (selected)
            ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // 底部：退出按钮
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    if (ImGui::Button(TR("nav.exit"), ImVec2(-1, 36)))
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
    auto t = [](const char *s)
    {ImGui::Spacing(); ImGui::TextColored(ImVec4(0.3f,0.8f,1.0f,1),"%s",s); ImGui::Separator(); };
    t(TR("menu.tools"));

    if (ImGui::Checkbox("ImGui Demo", &m_showDemo))
    {
        // 切换 demo 窗口
    }
    ImGui::Spacing();
    if (ImGui::Button(TR("menu.refresh"), ImVec2(180, 36)))
        checkPython();
}

// ==================== 设置页（语言） ====================
void App::renderSettingsPanel()
{
    auto t = [](const char *s)
    {ImGui::Spacing(); ImGui::TextColored(ImVec4(0.3f,0.8f,1.0f,1),"%s",s); ImGui::Separator(); };
    t(TR("settings.language"));

    bool en = (LangSys::I().lang() == Lang::English);
    bool cn = (LangSys::I().lang() == Lang::Chinese);
    if (ImGui::RadioButton(TR("settings.english"), en))
        LangSys::I().setLang(Lang::English);
    ImGui::Spacing();
    if (ImGui::RadioButton(TR("settings.chinese"), cn))
        LangSys::I().setLang(Lang::Chinese);
}

// ==================== 控制台 ====================
void App::renderLogArea()
{
    float ih = 28.0f; // 始终保留输入框高度
    float th = ImGui::GetFrameHeightWithSpacing() + 4;
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1), "%s", TR("console.title"));
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
                   "train | resume | predict | node | verify | menu | server");
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

    if (m_pythonOk)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", TR("status.python_ok"));
    else
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", TR("status.python_no"));
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();
    ImGui::Text("%s %s", TR("status.project"), m_projectRoot.c_str());

    // 进程状态显示在右下角
    ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetCursorPosX() - 100.0f);
    if (m_procRunning)
    {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s%s", TR("nav.status"), TR("nav.running"));
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
    auto t = [](const char *s)
    {ImGui::Spacing(); ImGui::TextColored(ImVec4(0.3f,0.8f,1.0f,1),"%s",s); ImGui::Separator(); };
    t(TR("env.python"));
    if (m_pythonOk)
    {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "  %s", TR("env.ready"));
        ImGui::SameLine();
        if (ImGui::SmallButton(TR("env.recheck")))
            checkPython();
        ImGui::Text("  %s %s", TR("env.version"), m_pythonVersion.c_str());
    }
    else
    {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "  %s", TR("env.nofound"));
        if (ImGui::Button(TR("env.detect"), ImVec2(120, 30)))
            checkPython();
    }
    t(TR("env.paths"));
    ImGui::Text("  %s %s", TR("env.root"), m_projectRoot.c_str());
    ImGui::Text("  %s %s", TR("env.cli"), m_cliRoot.c_str());
    ImGui::Text("  %s %s", TR("env.srv"), m_serverRoot.c_str());
    t(TR("env.actions"));
    if (ImGui::Button(TR("env.install_cli"), ImVec2(180, 35)))
        startProcess("install CLI", "-m pip install -r requirements.txt", m_cliRoot);
    ImGui::SameLine();
    if (ImGui::Button(TR("env.install_srv"), ImVec2(180, 35)))
        startProcess("install server", "-m pip install -r requirements.txt", m_serverRoot);
    ImGui::SameLine();
    if (ImGui::Button(TR("env.verify"), ImVec2(130, 35)))
        startProcess("verify env", "verify_env.py", m_projectRoot);
    t(TR("env.venv"));
    if (ImGui::Button(TR("env.venv"), ImVec2(220, 35)))
    {
        std::string o;
        int c = 0;
        runPython("-m venv .venv", m_projectRoot, o, c);
        if (c == 0)
            addSuccess(TR("env.venv_ok"));
        else
            addError(TR("env.venv_fail"));
    }

    // ---- Git 下载（整合进环境页） ----
    t(TR("git.title"));
    ImGui::Text("%s", TR("git.url"));
    ImGui::SetNextItemWidth(430);
    ImGui::InputText("##gu", m_gitUrl, sizeof(m_gitUrl));
    ImGui::Text("%s", TR("git.dir"));
    ImGui::SetNextItemWidth(340);
    ImGui::InputText("##gd", m_gitDir, sizeof(m_gitDir));
    ImGui::SameLine();
    if (ImGui::Button(TR("git.browse"), ImVec2(80, 0)))
    {
        char path[MAX_PATH] = {};
        BROWSEINFOA bi = {};
        bi.lpszTitle = "Select target directory";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
        if (pidl)
        {
            if (SHGetPathFromIDListA(pidl, path))
                strncpy_s(m_gitDir, path, sizeof(m_gitDir) - 1);
            CoTaskMemFree(pidl);
        }
    }
    ImGui::Spacing();
    if (m_gitCloning)
    {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", TR("git.cloning"));
    }
    else
    {
        if (ImGui::Button(TR("git.clone"), ImVec2(200, 36)))
        {
            std::string u(m_gitUrl), d(m_gitDir);
            if (!u.empty() && !d.empty())
                startGitClone(u, d);
            else
                addError(std::string(TR("git.fill")));
        }
        ImGui::SameLine();
        if (ImGui::Button(TR("git.check"), ImVec2(120, 36)))
        {
            if (_access(m_gitDir, 0) == 0)
            {
                addSuccess(std::string(TR("git.exists")));
                m_projectRoot = m_gitDir;
                m_cliRoot = std::string(m_gitDir) + "/src/cli";
                m_serverRoot = std::string(m_gitDir) + "/src/server";
                checkPython();
            }
            else
                addWarn(std::string(TR("git.none")));
        }
    }
}

// ==================== 服务端面板 ====================
void App::renderServerPanel()
{
    auto t = [](const char *s)
    {ImGui::Spacing(); ImGui::TextColored(ImVec4(0.3f,0.8f,1.0f,1),"%s",s); ImGui::Separator(); };
    t(TR("srv.title"));
    if (m_serverStatus == 1)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "  %s", TR("srv.on"));
    else if (m_serverStatus == 0)
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "  %s", TR("srv.off"));
    else
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "  %s", TR("srv.unknown"));
    if (ImGui::Button(TR("srv.start"), ImVec2(180, 40)))
        startProcess("start server", "runserver.py", m_serverRoot);
    ImGui::SameLine();
    if (ImGui::Button(TR("srv.check"), ImVec2(130, 40)))
    {
        m_serverStatus = m_http.ping() ? 1 : 0;
        if (m_serverStatus == 1)
            addSuccess(TR("srv.online"));
        else
            addWarn(TR("srv.offline"));
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("srv.api"), ImVec2(120, 30)))
        ShellExecuteA(0, "open", "http://127.0.0.1:13138/upload", 0, 0, SW_SHOW);
}

// ==================== CLI 面板 ====================
void App::renderCliPanel()
{
    auto t = [](const char *s)
    {ImGui::Spacing(); ImGui::TextColored(ImVec4(0.3f,0.8f,1.0f,1),"%s",s); ImGui::Separator(); };
    t(TR("cli.train"));
    if (ImGui::Button(TR("cli.btn_train"), ImVec2(180, 40)))
        startProcess("train", "main.py --mode 1", m_cliRoot);
    ImGui::SameLine();
    if (ImGui::Button(TR("cli.btn_resume"), ImVec2(180, 40)))
        startProcess("resume", "main.py --mode 1 --resume auto", m_cliRoot);
    ImGui::SameLine();
    if (ImGui::Button(TR("cli.btn_predict"), ImVec2(180, 40)))
        startProcess("predict", "main.py --mode 2", m_cliRoot);
    t(TR("cli.node"));
    if (ImGui::Button(TR("cli.btn_node"), ImVec2(180, 40)))
        startProcess("node", "main.py --mode 4", m_cliRoot);
    ImGui::SameLine();
    if (ImGui::Button(TR("cli.btn_verify"), ImVec2(180, 40)))
        startProcess("verify", "main.py --mode 3", m_cliRoot);
    ImGui::SameLine();
    if (ImGui::Button(TR("cli.btn_menu"), ImVec2(180, 40)))
        startProcess("menu", "main.py", m_cliRoot);
}