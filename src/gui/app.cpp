// 37ACUI — 37AC 桌面控制器
// 设计系统：樱花拿铁（与 37AC Web 前端 ac-tokens.css 对齐）
//
// 本文件负责：窗口/字体/样式初始化、配置与路径、Python 进程管理、
// 以及应用外壳（侧边栏 / 页头 / 页面容器 / 控制台 / 状态栏 / 模态框）。
// 各页面内容见 pages.cpp。

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include "app.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <direct.h>
#include <io.h>
#include <malloc.h>

// ===========================================================================
// 全局
// ===========================================================================
static GLFWwindow *g_window = nullptr;
static App *g_app = nullptr;

static void glfw_framebuffer_size_callback(GLFWwindow *, int w, int h)
{
    // 拖拽缩放时 Windows 进入模态循环，主循环拿不到控制权，这里补渲染一帧
    if (w > 0 && h > 0 && g_app)
        g_app->renderFrame();
    glfwPostEmptyEvent();
}

static void glfw_error_callback(int error, const char *desc)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, desc);
}

// 用户点关闭（×）：按设置隐藏到托盘，或保持默认的关闭（退出）行为
static void glfw_close_callback(GLFWwindow *window)
{

    if (!g_app)
        return;
    if (g_app->closeToTray())
    {
        glfwSetWindowShouldClose(window, GLFW_FALSE); // 取消关闭
        g_app->requestTrayHide();
    }
}

static void glfw_window_size_callback(GLFWwindow *, int w, int h)
{
    if (g_app)
        g_app->noteWindowSize(w, h);
}

static void glfw_window_pos_callback(GLFWwindow *, int x, int y)
{
    if (g_app)
        g_app->noteWindowPos(x, y);
}

static void glfw_window_maximize_callback(GLFWwindow *, int maximized)
{
    if (g_app)
        g_app->noteWindowMaximized(maximized != 0);
}

static std::wstring widenUtf8(const char *utf8)
{
    if (!utf8 || !utf8[0])
        return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (len <= 1)
        return std::wstring();
    std::wstring out((size_t)len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &out[0], len);
    return out;
}

// ===========================================================================
// 工具函数
// ===========================================================================
static bool fileExists(const std::string &p) { return _access(p.c_str(), 0) == 0; }

// 从 .exe 内嵌资源加载图标（任务栏 + 标题栏）
static void setWindowIcon()
{
    HICON hIcon = (HICON)LoadImage(GetModuleHandle(nullptr), MAKEINTRESOURCE(101),
                                   IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    if (!hIcon)
        return;
    ICONINFO ii = {};
    if (!GetIconInfo(hIcon, &ii))
        return;
    BITMAP bm = {};
    GetObject(ii.hbmColor, sizeof(bm), &bm);
    int w = bm.bmWidth, h = bm.bmHeight;
    if (w <= 0 || h <= 0)
    {
        DeleteObject(ii.hbmColor);
        DeleteObject(ii.hbmMask);
        return;
    }
    std::vector<unsigned char> pixels((size_t)w * h * 4);
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, w, h);
    HGDIOBJ oldBmp = SelectObject(hdcMem, hBmp);
    DrawIconEx(hdcMem, 0, 0, hIcon, w, h, 0, nullptr, DI_NORMAL);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    GetDIBits(hdcMem, hBmp, 0, h, pixels.data(), &bi, DIB_RGB_COLORS);
    for (int i = 0; i < w * h; i++)
        std::swap(pixels[i * 4 + 0], pixels[i * 4 + 2]);
    GLFWimage img = {w, h, pixels.data()};
    glfwSetWindowIcon(g_window, 1, &img);
    SelectObject(hdcMem, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    DeleteObject(ii.hbmColor);
    DeleteObject(ii.hbmMask);
}

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
            continue;
        }
        out += s[i];
    }
    return out;
}

// 执行命令并捕获输出（隐藏窗口）
static bool execCapture(const std::string &cmd, const std::string &cwd,
                        std::string &output, int &exitCode)
{
    output.clear();
    exitCode = -1;
    HANDLE hOutR = nullptr, hOutW = nullptr;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), 0, TRUE};
    if (!CreatePipe(&hOutR, &hOutW, &sa, 0))
        return false;
    SetHandleInformation(hOutR, HANDLE_FLAG_INHERIT, 0);
    HANDLE hInR = nullptr, hInW = nullptr;
    CreatePipe(&hInR, &hInW, &sa, 0);
    SetHandleInformation(hInW, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hOutW;
    si.hStdError = hOutW;
    si.hStdInput = hInR;

    std::string cmdLine = "cmd /c " + cmd;
    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessA(nullptr, &cmdLine[0], 0, 0, TRUE, CREATE_NO_WINDOW, 0,
                             (cwd.empty() ? nullptr : cwd.c_str()), &si, &pi);
    CloseHandle(hOutW);
    CloseHandle(hInR);
    CloseHandle(hInW);
    if (!ok)
    {
        CloseHandle(hOutR);
        output = "cannot start";
        return false;
    }
    CloseHandle(pi.hThread);

    char buf[4096];
    DWORD read = 0;
    while (ReadFile(hOutR, buf, sizeof(buf) - 1, &read, 0) && read > 0)
    {
        buf[read] = 0;
        output += buf;
    }
    CloseHandle(hOutR);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec = 0;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess);
    exitCode = (int)ec;
    return true;
}

// 用 curl 下载 GitHub 归档 zip，用 tar 解压后移动到目标目录
static int downloadArchiveZip(const std::string &url, const std::string &targetDir,
                              std::string &out)
{
    out.clear();
    std::string parent = targetDir;
    auto pos = parent.find_last_of("/\\");
    if (pos == std::string::npos)
        return -1;
    parent = parent.substr(0, pos);

    {
        std::string cur;
        for (size_t i = 0; i < parent.size(); i++)
        {
            cur += parent[i];
            if (parent[i] == '\\' || parent[i] == '/')
                if (cur.size() > 3)
                    _mkdir(cur.c_str());
        }
        _mkdir(parent.c_str());
    }

    const std::string zip = parent + "/37AC-master.zip";
    const std::string tmp = parent + "/_37ac_tmp";
    {
        std::string dl = "curl -L -sS -o \"" + zip + "\" \"" + url + "\"";
        int ec = -1;
        if (!execCapture(dl, "", out, ec) || ec != 0)
        {
            remove(zip.c_str());
            return ec;
        }
    }
    {
        std::string rm;
        int rc = 0;
        execCapture("rmdir /s /q \"" + tmp + "\"", "", rm, rc);
        _mkdir(tmp.c_str());
        std::string ex = "tar -xf \"" + zip + "\" -C \"" + tmp + "\"";
        int ec = -1;
        if (!execCapture(ex, "", out, ec) || ec != 0)
        {
            remove(zip.c_str());
            std::string rm2;
            int rc2 = 0;
            execCapture("rmdir /s /q \"" + tmp + "\"", "", rm2, rc2);
            return ec;
        }
    }
    remove(zip.c_str());

    bool moved = false;
    _finddata_t fd;
    intptr_t h = _findfirst((tmp + "/*").c_str(), &fd);
    if (h != -1)
    {
        do
        {
            if ((fd.attrib & _A_SUBDIR) && strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0)
            {
                if (MoveFileA((tmp + "/" + fd.name).c_str(), targetDir.c_str()))
                {
                    moved = true;
                    break;
                }
            }
        } while (_findnext(h, &fd) == 0);
        _findclose(h);
    }
    {
        std::string rm;
        int rc = 0;
        execCapture("rmdir /s /q \"" + tmp + "\"", "", rm, rc);
    }
    return moved ? 0 : -1;
}


static std::string openFileDialog(const char *filter, const char *title)
{
    char buf[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn))
        return std::string(buf);
    return std::string();
}

static std::string browseFolder(const char *title)
{
    char path[MAX_PATH] = {};
    BROWSEINFOA bi = {};
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (!pidl)
        return std::string();
    std::string out;
    if (SHGetPathFromIDListA(pidl, path))
        out = path;
    CoTaskMemFree(pidl);
    return out;
}

std::string PickFolder(const char *title) { return browseFolder(title); }
std::string PickFile(const char *filter, const char *title) { return openFileDialog(filter, title); }

// 按像素宽度裁剪文本（UTF-8 安全）
static std::string ellipsize(const std::string &text, ImFont *font, float maxW)
{
    if (!font || maxW <= 0.0f)
        return text;
    ImGui::PushFont(font);
    if (ImGui::CalcTextSize(text.c_str()).x <= maxW)
    {
        ImGui::PopFont();
        return text;
    }
    const char *ell = "…";
    std::string out;
    size_t pos = 0;
    while (pos < text.size())
    {
        unsigned char c = (unsigned char)text[pos];
        size_t step = 1;
        if ((c & 0x80) == 0)
            step = 1;
        else if ((c & 0xE0) == 0xC0)
            step = 2;
        else if ((c & 0xF0) == 0xE0)
            step = 3;
        else if ((c & 0xF8) == 0xF0)
            step = 4;
        std::string candidate = text.substr(0, pos + step) + ell;
        if (ImGui::CalcTextSize(candidate.c_str()).x > maxW)
            break;
        out = text.substr(0, pos + step);
        pos += step;
    }
    ImGui::PopFont();
    return out + ell;
}

// ---- 端口探测 ----
static void ensureWinsock()
{
    static bool initialized = []()
    {
        WSADATA d;
        WSAStartup(MAKEWORD(2, 2), &d);
        return true;
    }();
    (void)initialized;
}

static bool probePort(const std::string &host, int port, int timeoutMs = 150)
{
    ensureWinsock();
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET)
        return false;
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
    {
        closesocket(s);
        return false;
    }
    u_long nonBlocking = 1;
    ioctlsocket(s, FIONBIO, &nonBlocking);
    bool ok = false;
    if (connect(s, (sockaddr *)&addr, sizeof(addr)) == 0)
    {
        ok = true;
    }
    else
    {
        fd_set wf, ef;
        FD_ZERO(&wf);
        FD_ZERO(&ef);
        FD_SET(s, &wf);
        FD_SET(s, &ef);
        timeval tv = {0, timeoutMs * 1000};
        int sel = select(0, nullptr, &wf, &ef, &tv);
        if (sel > 0 && FD_ISSET(s, &wf))
        {
            int err = 0;
            int len = sizeof(err);
            getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
            ok = (err == 0);
        }
    }
    closesocket(s);
    return ok;
}

// ---- .env 读写 ----
static std::map<std::string, std::string> readEnvFile(const std::string &path)
{
    std::map<std::string, std::string> env;
    std::ifstream f(path.c_str());
    if (!f)
        return env;
    std::string line;
    while (std::getline(f, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        size_t b = line.find_first_not_of(" \t");
        if (b == std::string::npos || line[b] == '#')
            continue;
        size_t eq = line.find('=', b);
        if (eq == std::string::npos)
            continue;
        std::string key = line.substr(b, eq - b);
        std::string value = line.substr(eq + 1);
        size_t e = key.find_last_not_of(" \t");
        if (e != std::string::npos)
            key = key.substr(0, e + 1);
        size_t vb = value.find_first_not_of(" \t");
        size_t ve = value.find_last_not_of(" \t");
        if (vb == std::string::npos)
            value.clear();
        else
            value = value.substr(vb, ve - vb + 1);
        if (value.size() >= 2 && (value.front() == '"' || value.front() == '\''))
            value = value.substr(1, value.size() - 2);
        env[key] = value;
    }
    return env;
}

// ===========================================================================
// 构造 / 析构
// ===========================================================================
App::App() { m_mainThread = std::this_thread::get_id(); }

App::~App()
{
    m_tray.destroy();
    stopProcess();
    if (m_procThread.joinable())
        m_procThread.join();
    if (m_infoThread.joinable())
        m_infoThread.join();
}

// ===========================================================================
// 初始化
// ===========================================================================
bool App::init(const char *title, int width, int height)
{
    (void)title;
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return false;

    if (!createWindow(width, height, true))
        return false;
    setWindowIcon();

    // 最小化到系统托盘：托盘宿主窗口独立于 GLFW 窗口，始终可用
    m_trayAvailable = m_tray.create([this]() { requestTrayShow(); }, [this]() { requestTrayQuit(); });

    // 程序目录（exe 所在位置）
    {
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string p(exePath);
        auto pos = p.find_last_of("\\/");
        m_programDir = (pos == std::string::npos) ? p : p.substr(0, pos);
        updateProjectPaths();
    }

    // 主显示器居中
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

    loadConfig();
    refreshVenvState();

    // 防止窗口跑到屏幕外
    {
        bool onScreen = false;
        int monCount = 0;
        GLFWmonitor **mons = glfwGetMonitors(&monCount);
        for (int i = 0; i < monCount; i++)
        {
            int wx, wy, ww, wh;
            glfwGetMonitorWorkarea(mons[i], &wx, &wy, &ww, &wh);
            if (m_winX >= wx && m_winX < wx + ww - 100 && m_winY >= wy && m_winY < wy + wh - 30)
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
    glfwSetWindowSize(g_window, m_winW, m_winH);
    noteWindowPos(m_winX, m_winY);
    noteWindowSize(m_winW, m_winH);

    if (!createUi())
        return false;

    // 子进程统一 UTF-8 + 无缓冲，避免 pip 用 GBK 解码 requirement 报错
    _putenv_s("PYTHONIOENCODING", "utf-8");
    _putenv_s("PYTHONUNBUFFERED", "1");
    _putenv_s("PYTHONUTF8", "1");

    // 桥接脚本（内嵌，随 exe 分发）
    Bridge::ensureScript(m_programDir);

    checkPython();
    refreshEnv();
    refreshModelInfo();


    addInfo("[OK] 37ACUI started");
    addInfo(std::string("project: ") + m_projectRoot);
    if (m_pythonOk)
        addInfo(std::string("Python: ") + m_pythonVersion);
    else
        addWarn("Python not found — 初始化项目或检查 PATH 设置");

    g_app = this;
    return true;
}

// ===========================================================================
// 配置
// ===========================================================================
static const char *kConfigFile = "acui_settings.ini";

void App::saveConfig()
{
    if (g_window)
    {
        glfwGetWindowPos(g_window, &m_winX, &m_winY);
        glfwGetWindowSize(g_window, &m_winW, &m_winH);
    }
    std::string path = m_programDir + "/" + kConfigFile;
    FILE *f = fopen(path.c_str(), "w");
    if (!f)
        return;
    fprintf(f, "win_x=%d\n", m_winX);
    fprintf(f, "win_y=%d\n", m_winY);
    fprintf(f, "win_w=%d\n", m_winW);
    fprintf(f, "win_h=%d\n", m_winH);
    fprintf(f, "lang=%d\n", (int)LangSys::I().lang());
    fprintf(f, "page=%d\n", (int)m_page);
    fprintf(f, "auto_scroll=%d\n", m_autoScroll ? 1 : 0);
    fprintf(f, "console_open=%d\n", m_consoleOpen ? 1 : 0);
    fprintf(f, "console_plain=%d\n", m_consolePlain ? 1 : 0);
    fprintf(f, "log_ratio=%.3f\n", m_logRatio);
    fprintf(f, "dataset_choice=%d\n", m_datasetChoice);
    fprintf(f, "node_no_update=%d\n", m_nodeNoUpdate ? 1 : 0);
    fprintf(f, "close_to_tray=%d\n", m_closeToTray ? 1 : 0);
    fprintf(f, "program=%s\n", m_programDir.c_str());
    fprintf(f, "project=%s\n", m_projectRoot.c_str());
    fprintf(f, "git_url=%s\n", m_gitUrl);
    fclose(f);
}

void App::loadConfig()
{
    std::string path = m_programDir + "/" + kConfigFile;
    FILE *f = fopen(path.c_str(), "r");
    if (!f)
        return;
    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        int v = 0;
        float fv = 0.0f;
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
        else if (sscanf(line, "page=%d", &v) == 1)
            m_page = (Page)AcClamp(v, 0, (int)Page::Count - 1);
        else if (sscanf(line, "auto_scroll=%d", &v) == 1)
            m_autoScroll = (v != 0);
        else if (sscanf(line, "console_open=%d", &v) == 1)
            m_consoleOpen = (v != 0);
        else if (sscanf(line, "console_plain=%d", &v) == 1)
            m_consolePlain = (v != 0);
        else if (sscanf(line, "log_ratio=%f", &fv) == 1)
            m_logRatio = AcClamp(fv, 0.18f, 0.62f);
        else if (sscanf(line, "dataset_choice=%d", &v) == 1)
            m_datasetChoice = AcClamp(v, 0, 2);
        else if (sscanf(line, "node_no_update=%d", &v) == 1)
            m_nodeNoUpdate = (v != 0);
        else if (sscanf(line, "close_to_tray=%d", &v) == 1)
            m_closeToTray = (v != 0);
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

    // 旧配置兼容：.git 仓库地址迁移为归档 zip 地址
    {
        std::string u(m_gitUrl);
        if (u.size() > 4 && u.compare(u.size() - 4, 4, ".git") == 0)
        {
            u = u.substr(0, u.size() - 4) + "/archive/refs/heads/master.zip";
            strncpy_s(m_gitUrl, u.c_str(), sizeof(m_gitUrl) - 1);
        }
    }
    if (m_programDir.empty())
        updateProjectPaths();
    m_cliRoot = m_projectRoot + "/src/cli";
    m_serverRoot = m_projectRoot + "/src/server";
}

// ===========================================================================
// 路径与状态
// ===========================================================================
void App::updateProjectPaths()
{
    std::string base = m_programDir;
    while (!base.empty() && (base.back() == '/' || base.back() == '\\'))
        base.pop_back();
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_projectRoot = base + "/project/37AC";
    m_cliRoot = m_projectRoot + "/src/cli";
    m_serverRoot = m_projectRoot + "/src/server";
}

void App::refreshVenvState()
{
    std::string root;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        root = m_projectRoot;
    }
    bool exists = fileExists(root + "/.venv");
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_venvExists = exists;
}

void App::setPythonState(bool ok, const std::string &ver)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_pythonOk = ok;
    m_pythonVersion = ver;
}

void App::setProcName(const std::string &name)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_procName = name;
}

std::string App::procName()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_procName;
}

void App::closeProcStdin()
{
    std::lock_guard<std::mutex> lock(m_procStdinMutex);
    if (m_procStdinWrite)
    {
        CloseHandle((HANDLE)m_procStdinWrite);
        m_procStdinWrite = nullptr;
    }
}

std::string App::programDir()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_programDir;
}
std::string App::projectRoot()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_projectRoot;
}
std::string App::cliRoot()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_cliRoot;
}
std::string App::serverRoot()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_serverRoot;
}

std::string App::datasetDir()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return EnvOr(m_envCli, "DATASET_DIR", "W:/Img");
}

std::string App::croppedDir() { return cliRoot() + "/saves/dataset"; }
std::string App::modelDir() { return cliRoot() + "/saves/models"; }

std::string App::webHost()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    std::string h = EnvOr(m_envServer, "WEB_HOST", "127.0.0.1");
    if (h == "0.0.0.0" || h.empty())
        h = "127.0.0.1";
    return h;
}

int App::webPort()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return atoi(EnvOr(m_envServer, "WEB_PORT", "13138").c_str());
}

int App::tcpPort()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return atoi(EnvOr(m_envServer, "TCP_PORT", "13137").c_str());
}

std::string App::backendUrl()
{
    return "http://" + webHost() + ":" + std::to_string(webPort());
}

std::string App::frontendUrl()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    std::string url = EnvOr(m_envServer, "FRONTEND_URL", "http://127.0.0.1:8000");
    while (!url.empty() && url.back() == '/')
        url.pop_back();
    return url;
}

void App::refreshEnv()
{
    std::string root;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        root = m_projectRoot;
    }
    auto cli = readEnvFile(root + "/src/cli/.env");
    auto srv = readEnvFile(root + "/src/server/.env");
    auto web = readEnvFile(root + "/src/www/.env");
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_envCli = std::move(cli);
        m_envServer = std::move(srv);
        m_envWeb = std::move(web);
    }
    m_envDirty = false;
}

bool App::writeEnvValue(const std::string &dir, const std::string &key, const std::string &value)
{
    std::string path = dir + "/.env";
    std::vector<std::string> lines;
    bool replaced = false;
    {
        std::ifstream f(path.c_str());
        std::string line;
        while (std::getline(f, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const std::string prefix = key + "=";
            if (!replaced && line.compare(0, prefix.size(), prefix) == 0)
            {
                lines.push_back(prefix + value);
                replaced = true;
            }
            else
            {
                lines.push_back(line);
            }
        }
    }
    if (!replaced)
        lines.push_back(key + "=" + value);

    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    for (size_t i = 0; i < lines.size(); i++)
    {
        out << lines[i];
        if (i + 1 < lines.size())
            out << "\n";
    }
    out.close();
    return true;
}

AppStateSnapshot App::snapshot()
{
    AppStateSnapshot s;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        s.programDir = m_programDir;
        s.projectRoot = m_projectRoot;
        s.cliRoot = m_cliRoot;
        s.serverRoot = m_serverRoot;
        s.pythonOk = m_pythonOk;
        s.pythonVersion = m_pythonVersion;
        s.venvExists = m_venvExists;
        s.downloading = m_gitCloning;
        s.envCli = m_envCli;
        s.envServer = m_envServer;
        s.envWeb = m_envWeb;
    }
    {
        std::lock_guard<std::mutex> lock(m_infoMutex);
        s.model = m_modelInfo;
    }
    s.modelLoading = m_infoLoading.load();
    s.procRunning = m_procRunning.load();
    s.procName = procName();
    s.projectReady = fileExists(s.projectRoot) && fileExists(s.projectRoot + "/src/cli");
    s.webOnline = m_webOnline;
    s.tcpOnline = m_tcpOnline;
    s.webUrl = backendUrl();
    s.frontendUrl = frontendUrl();
    return s;
}

void App::probePorts(bool force)
{
    double now = ImGui::GetTime();
    if (!force && now - m_lastProbe < 4.0)
        return;
    m_lastProbe = now;
    std::string host = webHost();
    m_webOnline = probePort(host, webPort());
    m_tcpOnline = probePort(host, tcpPort());
}

void App::markDirty()
{
    m_uiDirty = true;
    // 托盘模式下界面已销毁，后台日志只入缓冲，不唤醒主循环
    if (g_window && !m_trayMode.load())
        glfwPostEmptyEvent();
}

// ===========================================================================
// 日志 / 通知
// ===========================================================================
void App::addLog(const std::string &msg, unsigned int)
{
    m_console.AddLine(ConsoleWidget::Level::Info, msg);
    markDirty();
}

void App::addInfo(const std::string &msg) { m_console.AddLine(ConsoleWidget::Level::Info, msg); markDirty(); }
void App::addSuccess(const std::string &msg) { m_console.AddLine(ConsoleWidget::Level::Success, msg); markDirty(); }
void App::addWarn(const std::string &msg) { m_console.AddLine(ConsoleWidget::Level::Warn, msg); markDirty(); }
void App::addError(const std::string &msg) { m_console.AddLine(ConsoleWidget::Level::Error, msg); markDirty(); }

void App::notify(UI::Tone tone, const std::string &text)
{
    if (std::this_thread::get_id() != m_mainThread)
    {
        std::lock_guard<std::mutex> lock(m_toastMutex);
        m_pendingToasts.emplace_back((int)tone, text);
        return;
    }
    UI::Notify(tone, text);
    markDirty();
}

void App::copyToClipboard(const std::string &text)
{
    ImGui::SetClipboardText(text.c_str());
}

void App::openUrl(const std::string &url)
{
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOW);
    addInfo("[open] " + url);
}

void App::openPath(const std::string &path)
{
    if (path.empty())
        return;
    if (!fileExists(path))
    {
        notify(UI::Tone::Warning, std::string("目录不存在: ") + path);
        return;
    }
    ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOW);
}

void App::openTextFile(const std::string &path)
{
    ShellExecuteA(nullptr, "open", "notepad.exe", path.c_str(), nullptr, SW_SHOW);
}

void App::goPage(Page p)
{
    m_page = p;
    m_envDirty = true;
}

// ===========================================================================
// Python 检测
// ===========================================================================
bool App::checkPython()
{
    std::string proot;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        proot = m_projectRoot;
    }
    std::string out;
    int ec = 0;
    bool ok = runPython("--version", proot, out, ec);
    if (ok && ec == 0)
    {
        std::string v;
        int vc = 0;
        runPython("-c \"import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}')\"",
                  proot, v, vc);
        if (vc == 0 && !v.empty())
            setPythonState(true, v);
        else
            setPythonState(true, out);
    }
    else
    {
        setPythonState(false, "not found");
    }
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_pythonOk;
}

bool App::runPython(const std::string &cmd, const std::string &cwd,
                    std::string &output, int &exitCode)
{
    output.clear();
    exitCode = -1;
    std::string workDir = cwd;
    if (!fileExists(workDir))
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        workDir = m_programDir;
    }

    std::string py = workDir + "/.venv/Scripts/python.exe";
    if (!fileExists(py))
    {
        py = workDir + "/../.venv/Scripts/python.exe";
        if (!fileExists(py))
        {
            py = workDir + "/../../.venv/Scripts/python.exe";
            if (!fileExists(py))
                py = "python";
        }
    }
    std::string fc = "chcp 65001>nul & ";
    fc += (py == "python") ? "python" : ("\"" + py + "\"");
    fc += " -u " + cmd + " 2>&1";
    if (!execCapture(fc, workDir, output, exitCode))
    {
        output = "cannot start";
        return false;
    }
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
        output.pop_back();
    return true;
}

// ===========================================================================
// 虚拟环境
// ===========================================================================
void App::createVenv(bool recreate)
{
    if (m_procRunning)
    {
        notify(UI::Tone::Warning, TR("toast.stop_first"));
        return;
    }
    m_procRunning = true;
    if (m_procThread.joinable())
        m_procThread.join();
    m_procThread = std::thread(&App::venvThreadFunc, this, recreate);
}

void App::venvThreadFunc(bool recreate)
{
    setProcName("create venv");
    std::string proot = projectRoot();
    std::string venvDir = proot + "/.venv";
    if (recreate && fileExists(venvDir))
    {
        addWarn(TR("env.venv_del_ok"));
        std::string o2;
        int c2 = 0;
        execCapture("rmdir /s /q \"" + venvDir + "\"", "", o2, c2);
    }
    else if (!recreate && fileExists(venvDir))
    {
        addWarn(TR("env.venv_exist"));
    }

    std::string o;
    int c = 0;
    runPython("-m venv .venv", proot, o, c);
    if (c == 0)
    {
        addSuccess(TR("env.venv_ok"));
        checkPython();
        notify(UI::Tone::Success, TR("env.venv_ok"));
    }
    else
    {
        addError(TR("env.venv_fail"));
        if (!o.empty())
            addError("[venv] " + o);
        if (o.find("Permission denied") != std::string::npos)
            addError(TR("env.venv_perm"));
        notify(UI::Tone::Danger, TR("env.venv_fail"));
    }
    refreshVenvState();
    setProcName("");
    m_procRunning = false;
    markDirty();
}

// ===========================================================================
// 一键初始化
// ===========================================================================
void App::startInitProject()
{
    if (m_procRunning)
    {
        notify(UI::Tone::Warning, TR("toast.stop_first"));
        return;
    }
    addInfo(std::string(TR("log.init")) + TR("env.init_start"));
    notify(UI::Tone::Info, TR("toast.initializing"));
    m_procRunning = true;
    if (m_procThread.joinable())
        m_procThread.join();
    std::string url(m_gitUrl);
    m_procThread = std::thread(&App::initProjectThreadFunc, this, url);
}

void App::initProjectThreadFunc(const std::string &url)
{
    setProcName("init project");
    std::string o;
    int c = 0;
    std::string proot = projectRoot();

    // 1. 下载源码
    if (!fileExists(proot))
    {
        addInfo(std::string(TR("log.init")) + TR("overview.btn_download") + ": " + url);
        std::string gco;
        int ec = downloadArchiveZip(url, proot, gco);
        {
            std::string l;
            for (char ch : gco)
            {
                if (ch == '\n')
                {
                    while (!l.empty() && l.back() == '\r')
                        l.pop_back();
                    if (!l.empty())
                        addInfo("  " + stripAnsi(l));
                    l.clear();
                }
                else
                {
                    l += ch;
                }
            }
            if (!l.empty())
                addInfo("  " + stripAnsi(l));
        }
        if (ec != 0)
        {
            addError(std::string(TR("log.init")) + TR("env.init_clone_fail") + std::to_string(ec));
            m_procRunning = false;
            markDirty();
            return;
        }
        addSuccess(std::string(TR("log.init")) + TR("env.init_clone_done"));
    }
    else
    {
        addInfo(TR("git.exists"));
    }

    // 2. 目录检查
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_cliRoot = m_projectRoot + "/src/cli";
        m_serverRoot = m_projectRoot + "/src/server";
    }
    refreshVenvState();
    if (!fileExists(cliRoot()) || !fileExists(serverRoot()))
        addWarn(std::string(TR("log.init")) + TR("env.init_missing_dirs"));
    else
        addSuccess(std::string(TR("log.init")) + TR("env.init_dirs_ok"));

    // 3. 虚拟环境
    addInfo(std::string(TR("log.init")) + TR("env.venv") + "…");
    o.clear();
    c = 0;
    runPython("-m venv .venv", proot, o, c);
    if (c != 0)
    {
        addError(TR("env.venv_fail"));
        if (!o.empty())
            addError("[venv] " + o);
        m_procRunning = false;
        markDirty();
        return;
    }
    addSuccess(TR("env.venv_ok"));
    refreshVenvState();

    // 4. 依赖
    addInfo(std::string(TR("log.init")) + TR("env.install_cli") + "…");
    o.clear();
    c = 0;
    runPython("-m pip install -r requirements.txt", cliRoot(), o, c);
    if (c != 0)
    {
        addError(std::string(TR("log.init")) + TR("env.init_cli_fail"));
        if (!o.empty())
            addError(std::string(TR("log.pip")) + o);
    }
    else
    {
        addSuccess(std::string(TR("log.init")) + TR("env.init_cli_ok"));
    }

    addInfo(std::string(TR("log.init")) + TR("env.install_srv") + "…");
    o.clear();
    c = 0;
    runPython("-m pip install -r requirements.txt", serverRoot(), o, c);
    if (c != 0)
    {
        addError(std::string(TR("log.init")) + TR("env.init_srv_fail"));
        if (!o.empty())
            addError(std::string(TR("log.pip")) + o);
    }
    else
    {
        addSuccess(std::string(TR("log.init")) + TR("env.init_srv_ok"));
    }

    // 5. .env
    addInfo(std::string(TR("log.init")) + TR("env.init_env") + "…");
    {
        std::vector<std::string> comps = {cliRoot(), serverRoot(), proot + "/src/www"};
        for (auto &d : comps)
        {
            std::string ex = d + "/.env.example";
            std::string env = d + "/.env";
            if (fileExists(ex) && !fileExists(env))
            {
                if (CopyFileA(ex.c_str(), env.c_str(), FALSE))
                    addSuccess(std::string(TR("log.init")) + "[env] " + d + " → .env");
            }
        }
    }

    // 6. 验证
    addInfo(std::string(TR("log.init")) + TR("env.verify") + "…");
    o.clear();
    c = 0;
    runPython("verify_env.py", proot, o, c);
    if (c == 0 && !o.empty())
        addInfo("  " + o);
    else
        addWarn(std::string(TR("log.init")) + TR("env.init_verify_warn"));
    checkPython();

    m_envDirty = true;
    m_infoRefreshRequested = true;
    addSuccess(std::string(TR("log.init")) + TR("env.init_done"));
    notify(UI::Tone::Success, TR("env.init_done"));
    setProcName("");
    m_procRunning = false;
    markDirty();
}

// ===========================================================================
// 通用进程
// ===========================================================================
void App::procThreadFunc(const std::string &name, const std::string &cmd,
                         const std::string &cwd)
{
    addInfo(std::string("[start] ") + name + ": " + cmd);

    std::string py = cwd + "/.venv/Scripts/python.exe";
    if (!fileExists(py))
    {
        py = cwd + "/../.venv/Scripts/python.exe";
        if (!fileExists(py))
        {
            py = cwd + "/../../.venv/Scripts/python.exe";
            if (!fileExists(py))
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
    {
        std::lock_guard<std::mutex> lock(m_procStdinMutex);
        m_procStdinWrite = hInW;
    }

    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hOutW;
    si.hStdError = hOutW;
    si.hStdInput = hInR;
    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessA(nullptr, &fc[0], 0, 0, TRUE, CREATE_NO_WINDOW, 0,
                             cwd.c_str(), &si, &pi);
    CloseHandle(hOutW);
    CloseHandle(hInR);

    if (!ok)
    {
        addError(std::string("[error] ") + name + " code=" + std::to_string(GetLastError()));
        CloseHandle(hOutR);
        closeProcStdin();
        m_procRunning = false;
        m_procPid = 0;
        markDirty();
        return;
    }
    m_procPid = pi.dwProcessId;
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
                {
                    break;
                }
            }
            else
            {
                DWORD ec2 = 0;
                if (!GetExitCodeProcess(pi.hProcess, &ec2) || ec2 != STILL_ACTIVE)
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        else
        {
            break;
        }
    }
    while (!part.empty() && (part.back() == '\n' || part.back() == '\r'))
        part.pop_back();
    if (!part.empty())
        addInfo("  " + stripAnsi(part));

    DWORD ec = 0;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess);
    CloseHandle(hOutR);
    bool success = (ec == 0);
    if (success)
        addSuccess(std::string("[done] ") + name + " exit=" + std::to_string(ec));
    else
        addError(std::string("[done] ") + name + " exit=" + std::to_string(ec));

    closeProcStdin();
    setProcName("");
    m_procRunning = false;
    m_procPid = 0;

    // 桥接动作结束后刷新模型信息 + 弹出结果通知
    if (name.rfind("bridge: ", 0) == 0)
    {
        std::string label = name.substr(8);
        m_infoRefreshRequested = true;
        char msg[512];
        if (success)
        {
            snprintf(msg, sizeof(msg), TR("toast.bridge_ok"), label.c_str());
            notify(UI::Tone::Success, msg);
        }
        else
        {
            snprintf(msg, sizeof(msg), TR("toast.bridge_fail"), label.c_str(), "详见控制台");
            notify(UI::Tone::Danger, msg);
        }
    }
    markDirty();
}

void App::startProcess(const std::string &name, const std::string &cmd,
                       const std::string &cwd)
{
    if (m_procRunning)
    {
        notify(UI::Tone::Warning, TR("toast.stop_first"));
        return;
    }
    if (!fileExists(cwd))
    {
        notify(UI::Tone::Danger, TR("toast.need_project"));
        return;
    }
    closeProcStdin();
    setProcName(name);
    m_procRunning = true;
    if (m_procThread.joinable())
        m_procThread.join();
    m_procThread = std::thread(&App::procThreadFunc, this, name, cmd, cwd);
    markDirty();
}

void App::stopProcess()
{
    if (!m_procRunning)
        return;
    addWarn("[warn] terminating…");
    m_procRunning = false;
    setProcName("");
    closeProcStdin();
    unsigned long pid = m_procPid.load();
    if (pid != 0)
    {
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
        if (h)
        {
            TerminateProcess(h, 1);
            CloseHandle(h);
        }
    }
    notify(UI::Tone::Warning, TR("console.stopped"));
    markDirty();
}

// ===========================================================================
// 业务动作
// ===========================================================================
void App::startGitDownload()
{
    if (m_procRunning)
    {
        notify(UI::Tone::Warning, TR("toast.stop_first"));
        return;
    }
    std::string url(m_gitUrl);
    std::string dir = projectRoot();
    addInfo(std::string("[download] ") + url);
    notify(UI::Tone::Info, TR("toast.downloading"));
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_gitCloning = true;
    }
    setProcName("download 37AC");
    m_procRunning = true;
    if (m_procThread.joinable())
        m_procThread.join();
    m_procThread = std::thread([this, url, dir]()
                               {
        if (fileExists(dir)) {
            addWarn("[download] removing old folder…");
            std::string oo; int cc = 0;
            execCapture("rmdir /s /q \"" + dir + "\"", "", oo, cc);
        }
        std::string out; int ec = downloadArchiveZip(url, dir, out);
        {
            std::string l;
            for (char ch : out) {
                if (ch == '\n') {
                    while (!l.empty() && l.back() == '\r') l.pop_back();
                    if (!l.empty()) addInfo("  " + stripAnsi(l));
                    l.clear();
                } else l += ch;
            }
            if (!l.empty()) addInfo("  " + stripAnsi(l));
        }
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (ec == 0) {
                m_projectRoot = dir;
                m_cliRoot = dir + "/src/cli";
                m_serverRoot = dir + "/src/server";
            }
            m_gitCloning = false;
        }
        if (ec == 0) {
            addSuccess("[download] success");
            notify(UI::Tone::Success, TR("env.init_clone_done"));
            refreshVenvState();
            checkPython();
            m_envDirty = true;
            m_infoRefreshRequested = true;
        } else {
            addError("[download] failed exit=" + std::to_string(ec));
            notify(UI::Tone::Danger, "[download] failed");
        }
        setProcName("");
        m_procRunning = false;
        markDirty(); });
}

void App::startTrain(bool resume)
{
    if (m_procRunning)
    {
        notify(UI::Tone::Warning, TR("toast.stop_first"));
        return;
    }
    std::string root = projectRoot();
    if (!fileExists(root + "/src/cli"))
    {
        notify(UI::Tone::Danger, TR("toast.need_project"));
        return;
    }
    std::string ds = (m_datasetChoice == 1) ? croppedDir() : datasetDir();
    if (!fileExists(ds))
    {
        notify(UI::Tone::Danger, std::string("数据集不存在: ") + ds);
        addError("[train] dataset not found: " + ds);
        return;
    }
    std::string cmd = "main.py train --dataset \"" + ds + "\"";
    if (m_datasetChoice == 2)
        cmd += " --yolo-crop";
    if (resume)
        cmd += " --resume";
    startProcess(resume ? "train (resume)" : "train", cmd, cliRoot());
    notify(UI::Tone::Info, TR("toast.train_started"));
}

void App::startPredict(bool llm)
{
    if (m_procRunning)
    {
        notify(UI::Tone::Warning, TR("toast.stop_first"));
        return;
    }
    std::string image = openFileDialog(
        "图片文件 (*.jpg;*.jpeg;*.png;*.webp;*.bmp)\0*.jpg;*.jpeg;*.png;*.webp;*.bmp\0所有文件 (*.*)\0*.*\0",
        "选择要识别的图片");
    if (image.empty())
        return;
    if (llm)
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (EnvOr(m_envCli, "LLM_RECOGNITION_ENABLED", "False") != "True")
            notify(UI::Tone::Warning, TR("toast.llm_disabled"));
    }
    std::string cmd = "main.py predict";
    if (llm)
        cmd += " --llm";
    cmd += " --image \"" + image + "\"";
    startProcess(llm ? "predict (LLM)" : "predict", cmd, cliRoot());
}

void App::startNodeService()
{
    if (m_procRunning)
    {
        notify(UI::Tone::Warning, TR("toast.stop_first"));
        return;
    }
    std::string cmd = "main.py node";
    if (m_nodeNoUpdate)
        cmd += " --no-model-update";
    startProcess("node", cmd, cliRoot());
    notify(UI::Tone::Info, TR("toast.node_started"));
}

void App::startServer()
{
    if (m_procRunning)
    {
        notify(UI::Tone::Warning, TR("toast.stop_first"));
        return;
    }
    startProcess("server", "runserver.py", serverRoot());
    notify(UI::Tone::Info, TR("toast.server_started"));
}

void App::runBridge(const std::string &label, const std::string &args)
{
    if (m_procRunning)
    {
        notify(UI::Tone::Warning, TR("toast.stop_first"));
        return;
    }
    if (!snapshot().pythonOk)
    {
        notify(UI::Tone::Danger, TR("toast.need_python"));
        return;
    }
    std::string script = Bridge::ensureScript(programDir());
    startProcess("bridge: " + label, Bridge::command(script, args), cliRoot());
}

void App::executeConsoleCommand(const std::string &raw)
{
    std::string cmd = raw;
    // 去掉首尾空白
    size_t b = cmd.find_first_not_of(" \t");
    size_t e = cmd.find_last_not_of(" \t");
    if (b == std::string::npos)
        return;
    cmd = cmd.substr(b, e - b + 1);

    // 有进程在跑时，输入直接转发到子进程 stdin（驱动 37AC 的交互菜单）
    if (m_procRunning)
    {
        HANDLE h = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_procStdinMutex);
            h = (HANDLE)m_procStdinWrite;
        }
        if (h)
        {
            std::string line = cmd + "\n";
            DWORD w = 0;
            WriteFile(h, line.c_str(), (DWORD)line.size(), &w, nullptr);
            return;
        }
    }

    std::string lower = cmd;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return (char)tolower(c); });

    if (lower == "help" || lower == "?")
    {
        addInfo("可用命令：");
        addInfo("  train      从头训练（使用当前数据集选择）");
        addInfo("  resume     继续训练当前模型");
        addInfo("  predict    识别角色（交互输入图片路径）");
        addInfo("  node       启动识别节点");
        addInfo("  server     启动 Flask 服务端");
        addInfo("  verify     验证数据集图像");
        addInfo("  crop       YOLO 裁剪数据集");
        addInfo("  compress   压缩已裁剪数据集");
        addInfo("  jpeg       数据集转 JPEG");
        addInfo("  model      进入 37AC 交互菜单");
        addInfo("  info       重新读取模型信息");
        addInfo("  clear      清空控制台");
        return;
    }
    if (lower == "clear")
    {
        m_console.ClearLog();
        return;
    }
    if (lower == "info")
    {
        refreshModelInfo();
        notify(UI::Tone::Info, TR("toast.info_loading"));
        return;
    }
    if (lower == "train")
    {
        startTrain(false);
        return;
    }
    if (lower == "resume")
    {
        startTrain(true);
        return;
    }
    if (lower == "node")
    {
        startNodeService();
        return;
    }
    if (lower == "server")
    {
        startServer();
        return;
    }
    if (lower == "verify")
    {
        runBridge(TR("action.verify"), "dataset-verify");
        return;
    }
    if (lower == "crop")
    {
        runBridge(TR("action.crop"), "dataset-crop");
        return;
    }
    if (lower == "compress")
    {
        runBridge(TR("action.compress"), "dataset-compress");
        return;
    }
    if (lower == "jpeg")
    {
        runBridge(TR("action.jpeg"), "dataset-jpeg");
        return;
    }
    if (lower == "model" || lower == "menu")
    {
        startProcess("menu", "main.py", cliRoot());
        return;
    }
    if (lower == "predict" || lower == "predict --llm")
    {
        startProcess("predict", "main.py " + cmd.substr(lower.find("predict")), cliRoot());
        return;
    }

    addWarn("[warn] 未知命令：" + cmd + "（输入 help 查看可用命令）");
}

// ===========================================================================
// 模型信息后台线程
// ===========================================================================
void App::refreshModelInfo()
{
    if (m_infoLoading.exchange(true))
        return;
    if (m_infoThread.joinable())
        m_infoThread.join();
    m_infoThread = std::thread(&App::infoThreadFunc, this);
}

void App::infoThreadFunc()
{
    Bridge::Info info;
    std::string script = Bridge::ensureScript(programDir());
    std::string cli = cliRoot();
    if (!fileExists(cli))
    {
        info.model.ok = false;
        info.model.error = TR("toast.need_project");
    }
    else
    {
        std::string out;
        int ec = 0;
        bool ran = runPython(Bridge::command(script, "info"), cli, out, ec);
        if (!ran || !Bridge::parseInfo(out, info))
        {
            info.model.ok = false;
            if (info.model.error.empty())
                info.model.error = ran ? "模型信息解析失败" : TR("toast.need_python");
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_infoMutex);
        m_modelInfo = info;
    }
    m_infoLoading = false;
    glfwPostEmptyEvent();
}

// ===========================================================================
// 界面上下文（可销毁 / 重建）
// 最小化到托盘时整块释放：ImGui 上下文、字体图集（GPU 纹理 + 字体文件缓存）
// ===========================================================================
bool App::createWindow(int width, int height, bool visible)
{
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);

    g_window = glfwCreateWindow(width, height, "37ACUI", nullptr, nullptr);
    if (!g_window)
        return false;

    glfwSetWindowSizeLimits(g_window, 980, 640, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetFramebufferSizeCallback(g_window, glfw_framebuffer_size_callback);
    glfwSetWindowSizeCallback(g_window, glfw_window_size_callback);
    glfwSetWindowPosCallback(g_window, glfw_window_pos_callback);
    glfwSetWindowMaximizeCallback(g_window, glfw_window_maximize_callback);
    // 注意：不再拦截最小化（−），点 − 走系统默认行为 -> 任务栏
    glfwSetWindowCloseCallback(g_window, glfw_close_callback);
    noteWindowSize(width, height);

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);
    return true;
}

bool App::createUi()
{
    if (ImGui::GetCurrentContext())
        return true;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // 手动管理布局 ini，避免周期性写盘

    Theme::applyStyle();
    m_fonts = Theme::loadFonts();

    std::string iniPath = m_programDir + "/acui.ini";
    ImGui::LoadIniSettingsFromDisk(iniPath.c_str());

    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    return true;
}

void App::destroyUi()
{
    if (!ImGui::GetCurrentContext())
        return;

    // 先保存窗口布局，重建后原样恢复
    std::string iniPath = m_programDir + "/acui.ini";
    ImGui::SaveIniSettingsToDisk(iniPath.c_str());

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    Theme::releaseFontData(); // 字体文件缓存（雅黑约 20MB）也一并还给系统
}

// 托盘模式入口：隐藏窗口 + 释放全部 GUI 资源 + 收缩工作集
void App::enterTrayMode()
{
    if (!g_window || m_trayMode.load())
        return;

    // 没有托盘可用时退回普通最小化，避免窗口消失后无法找回
    if (!m_trayAvailable)
    {
        glfwRestoreWindow(g_window);
        return;
    }
    if (m_trayMode.exchange(true))
        return;


    addInfo(std::string("[tray] ") + TR("tray.log_enter"));
    destroyUi();
    m_console.CompactMemory(); // 日志缓存的多余容量

    // 关闭中的模态框在新上下文里没有对应弹窗，避免“幽灵状态”
    m_confirm.active = false;
    m_confirm.popupOpened = false;
    m_confirm.onOk = nullptr;
    m_input.active = false;
    m_input.popupOpened = false;
    m_input.onOk = nullptr;

    // 记录窗口状态：最小化后 glfwGetWindowSize 会返回 0x0，
    // 因此使用尺寸/位置回调缓存下来的最后一次有效几何。
    if (m_trayWinW <= 0 || m_trayWinH <= 0)
    {
        m_trayWinW = m_winW;
        m_trayWinH = m_winH;
    }
    // 最大化状态在最小化后会被 GLFW 重置，这里用回调缓存的状态
    m_winX = m_trayWinX;
    m_winY = m_trayWinY;
    m_winW = m_trayWinW;
    m_winH = m_trayWinH;

    // 连窗口与 OpenGL 上下文一起销毁：驱动为桌面/上下文分配的私有内存也随之归还。
    // 恢复时会重建窗口 + 上下文（隐藏时用 GLFW_VISIBLE=false，避免闪窗）。
    glfwDestroyWindow(g_window);
    g_window = nullptr;

    std::wstring tip = widenUtf8(TR("tray.tip"));
    std::wstring balloon = widenUtf8(TR("tray.balloon"));
    m_tray.show(tip.c_str(), balloon.c_str());
    trimWorkingSet();
}

// 从托盘恢复：重建 UI 上下文（字体、图集、布局）并显示窗口
void App::leaveTrayMode()
{
    if (!m_trayMode.exchange(false))
        return;

    m_tray.hide();
    if (!createWindow(m_trayWinW, m_trayWinH, false))
    {
        // 极端情况（驱动拒绝新建上下文）：重新显示托盘，保持程序可退出
        m_tray.show();
        return;
    }
    glfwSetWindowPos(g_window, m_trayWinX, m_trayWinY);
    setWindowIcon();

    createUi();
    glfwShowWindow(g_window);
    if (m_trayWinMaximized)
        glfwMaximizeWindow(g_window);
    glfwFocusWindow(g_window);

    m_envDirty = true;
    m_uiDirty = true;
    probePorts(true);
    addInfo(std::string("[tray] ") + TR("tray.log_restore"));
    glfwPostEmptyEvent();
}

void App::trimWorkingSet()
{
    // 1) 把 CRT 堆里已释放的页还给系统；2) 收缩工作集（任务管理器里立刻可见）
    _heapmin();
    HeapCompact(GetProcessHeap(), 0);
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

void App::noteWindowSize(int w, int h)
{
    if (w <= 0 || h <= 0 || !g_window)
        return;
    // 最大化/最小化时的尺寸不是「还原尺寸」，不参与缓存
    if (glfwGetWindowAttrib(g_window, GLFW_MAXIMIZED) ||
        glfwGetWindowAttrib(g_window, GLFW_ICONIFIED))
        return;
    m_trayWinW = w;
    m_trayWinH = h;
}

void App::noteWindowMaximized(bool maximized) { m_trayWinMaximized = maximized; }

void App::noteWindowPos(int x, int y)
{
    if (x <= -10000 || y <= -10000 || !g_window)
        return;
    if (glfwGetWindowAttrib(g_window, GLFW_MAXIMIZED) ||
        glfwGetWindowAttrib(g_window, GLFW_ICONIFIED))
        return;
    m_trayWinX = x;
    m_trayWinY = y;
}

void App::requestTrayHide()
{
    m_pendingTrayHide = true;
    if (g_window)
        glfwPostEmptyEvent();
}

void App::requestTrayShow()
{
    m_pendingTrayShow = true;
    if (g_window)
        glfwPostEmptyEvent();
}

void App::requestTrayQuit()
{
    m_pendingTrayQuit = true;
    if (g_window)
        glfwPostEmptyEvent();
}

// ===========================================================================
// 渲染一帧
// ===========================================================================
void App::renderFrame()
{
    if (m_trayMode.load() || !ImGui::GetCurrentContext() || !g_window)
        return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    render();
    ImGui::Render();
    int dw, dh;
    glfwGetFramebufferSize(g_window, &dw, &dh);
    glViewport(0, 0, dw, dh);
    ImVec4 bg = ImGui::ColorConvertU32ToFloat4(Theme::T.bg);
    glClearColor(bg.x, bg.y, bg.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(g_window);
}

// ===========================================================================
// 主循环
// ===========================================================================
void App::run()
{
    bool busy = false;

    while (!m_quitRequested.load())
    {
        if (g_window && glfwWindowShouldClose(g_window))
        {
            m_quitRequested = true;
            break;
        }

        // 托盘菜单 / 最小化回调写入的状态（都在主线程处理）
        if (m_pendingTrayQuit.exchange(false))
        {
            m_quitRequested = true;
            continue;
        }
        if (m_pendingTrayHide.exchange(false))
        {
            enterTrayMode();
            continue;
        }
        if (m_pendingTrayShow.exchange(false))
        {
            leaveTrayMode();
            busy = false;
            continue;
        }

        // 托盘模式：不渲染、不探测、不跑动画，只阻塞等待消息
        if (m_trayMode.load())
        {
            glfwWaitEvents();
            continue;
        }

        // 事件驱动：静止时零 CPU；有动画/输入焦点时保持 60fps
        if (busy)
            glfwWaitEventsTimeout(1.0 / 60.0);
        else
            glfwWaitEvents();

        // 托盘模式 / 窗口重建期间没有可渲染对象
        if (m_trayMode.load() || !g_window || !ImGui::GetCurrentContext())
            continue;
        ImGuiIO &io = ImGui::GetIO();

        int fw = 0, fh = 0;
        glfwGetFramebufferSize(g_window, &fw, &fh);
        if (fw == 0 || fh == 0)
            continue;

        renderFrame();
        m_uiDirty = false;
        busy = Theme::animating() || io.WantTextInput;
    }

    m_tray.destroy();
    stopProcess();
    if (m_procThread.joinable())
        m_procThread.join();
    if (m_infoThread.joinable())
        m_infoThread.join();
    saveConfig();

    // 可能是在托盘模式（UI 已销毁）下退出，需要按需处理
    if (ImGui::GetCurrentContext())
    {
        std::string iniPath = m_programDir + "/acui.ini";
        ImGui::SaveIniSettingsToDisk(iniPath.c_str());
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    Theme::releaseFontData();
    if (g_window)
    {
        glfwDestroyWindow(g_window);
        g_window = nullptr;
    }
    glfwTerminate();
}

// ===========================================================================
// 渲染：总入口
// ===========================================================================
void App::render()
{
    Theme::beginFrame();
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 display = io.DisplaySize;

    if (m_infoRefreshRequested.exchange(false) && !m_infoLoading.load())
        refreshModelInfo();
    if (m_envDirty)
        refreshEnv();
    probePorts(false);

    // 后台线程投递的 Toast
    {
        std::lock_guard<std::mutex> lock(m_toastMutex);
        for (auto &t : m_pendingToasts)
            UI::Notify((UI::Tone)t.first, t.second);
        m_pendingToasts.clear();
    }


    renderSidebar(display);
    renderMain(display);
    renderStatusBar(display);
    UI::RenderToasts();
}

// ===========================================================================
// 侧边栏
// ===========================================================================
void App::drawBrand()
{
    const Theme::Tokens &T = Theme::T;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float box = 42.0f;

    dl->AddRectFilled(p, ImVec2(p.x + box, p.y + box), T.pink400, 13.0f);
    dl->AddRectFilled(ImVec2(p.x + 2, p.y + 2), ImVec2(p.x + box - 2, p.y + box * 0.55f),
                      Theme::withAlpha(IM_COL32(255, 255, 255, 255), 0.16f), 11.0f);
    dl->AddText(Theme::fonts().display, 26.0f, ImVec2(p.x + 7.0f, p.y + 6.0f),
                IM_COL32(255, 255, 255, 255), "37");

    dl->AddText(Theme::fonts().heading, 18.0f, ImVec2(p.x + box + 12.0f, p.y + 3.0f),
                T.ink900, "37ACUI");
    dl->AddText(Theme::fonts().caption, 13.0f, ImVec2(p.x + box + 12.0f, p.y + 25.0f),
                T.ink500, TR("app.subtitle"));

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + box + 16.0f));
}

bool App::navItem(const char *id, UI::Icon icon, const char *label, bool selected,
                  const char *badge)
{
    const Theme::Tokens &T = Theme::T;
    const Theme::Fonts &F = Theme::fonts();
    const float h = 38.0f;
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();

    bool clicked = ImGui::InvisibleButton(id, ImVec2(w, h));
    ImGuiID wid = ImGui::GetItemID();
    bool hovered = ImGui::IsItemHovered();

    float st = Theme::anim(wid, selected ? 1.0f : 0.0f, 15.0f);
    float hv = Theme::anim(wid ^ 0x51ED2701u, hovered ? 1.0f : 0.0f, 15.0f);

    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImU32 bg = Theme::mix(Theme::withAlpha(T.surface2, 0.0f), T.surface2, hv);
    bg = Theme::mix(bg, T.pink50, st);
    if (((bg >> IM_COL32_A_SHIFT) & 0xFF) > 0)
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, 12.0f);

    if (st > 0.02f)
        dl->AddRectFilled(ImVec2(p.x + 2.0f, p.y + 11.0f), ImVec2(p.x + 5.0f, p.y + h - 11.0f),
                          Theme::withAlpha(T.pink500, st), 1.5f);

    ImU32 iconCol = Theme::mix(T.ink500, T.pink600, st);
    UI::DrawIcon(dl, icon, ImVec2(p.x + 22.0f, p.y + h * 0.5f), 17.0f, iconCol);

    float textX = p.x + 38.0f;
    ImFont *font = (st > 0.5f && F.bodyBold) ? F.bodyBold : F.body;
    ImU32 textCol = Theme::mix(T.ink700, T.pink700, st);
    ImVec2 after = ImGui::GetCursorScreenPos();
    if (font)
        ImGui::PushFont(font);
    dl->AddText(font, font ? font->LegacySize : 16.0f,
                ImVec2(textX, p.y + (h - ImGui::GetTextLineHeight()) * 0.5f), textCol, label);
    if (font)
        ImGui::PopFont();

    if (badge && badge[0])
    {
        ImFont *bf = F.caption ? F.caption : F.body;
        float bw = bf ? bf->CalcTextSizeA(bf->LegacySize, FLT_MAX, 0.0f, badge).x : 0.0f;
        dl->AddText(bf, bf ? bf->LegacySize : 13.0f,
                    ImVec2(p.x + w - bw - 10.0f, p.y + (h - 14.0f) * 0.5f), T.ink300, badge);
    }

    ImGui::SetCursorScreenPos(after);
    return clicked;
}

void App::renderSidebar(const ImVec2 &display)
{
    const Theme::Tokens &T = Theme::T;
    AppStateSnapshot s = snapshot();
    float w = T.sidebarW;
    float h = display.y - T.statusBarH;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 18));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.surface);
    ImGui::Begin("##sidebar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    drawBrand();
    UI::SectionLabel(TR("nav.workspace"));
    ImGui::Dummy(ImVec2(0, 6.0f));

    std::string modelBadge = s.model.model.ok && !s.model.model.version.empty()
                                 ? s.model.model.version
                                 : "";
    struct NavDef
    {
        Page page;
        UI::Icon icon;
        const char *label;
    };
    const NavDef navs[] = {
        {Page::Overview, UI::Icon::Grid, TR("nav.overview")},
        {Page::Model, UI::Icon::Cube, TR("nav.model")},
        {Page::Dataset, UI::Icon::Database, TR("nav.dataset")},
        {Page::Node, UI::Icon::Network, TR("nav.node")},
        {Page::Server, UI::Icon::Server, TR("nav.server")},
    };
    for (const auto &n : navs)
    {
        const char *badge = (n.page == Page::Model && !modelBadge.empty()) ? modelBadge.c_str() : nullptr;
        if (navItem(("##nav" + std::to_string((int)n.page)).c_str(), n.icon, n.label,
                    m_page == n.page, badge))
            goPage(n.page);
    }

    ImGui::Dummy(ImVec2(0, 10.0f));
    UI::SectionLabel(TR("nav.system"));
    ImGui::Dummy(ImVec2(0, 6.0f));
    const NavDef sys[] = {
        {Page::Env, UI::Icon::Gear, TR("nav.env")},
        {Page::Settings, UI::Icon::Sliders, TR("nav.settings")},
    };
    for (const auto &n : sys)
    {
        if (navItem(("##nav" + std::to_string((int)n.page)).c_str(), n.icon, n.label,
                    m_page == n.page, nullptr))
            goPage(n.page);
    }

    // 底部：环境小结 + 退出（固定贴底：96 卡片 + 12 间距 + 38 按钮 + 18 内边距）
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 164.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 11));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.surface2);
    // 无边框子窗口默认会把 WindowPadding 清 0，必须显式要求使用内边距，
    // 否则状态圆点会贴在卡片的圆角上。
    ImGui::BeginChild("##navstatus", ImVec2(0, 96),
                      ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    auto row = [&](bool ok, const char *label, const char *value)
    {
        UI::StatusDot(ok ? UI::Tone::Success : UI::Tone::Danger, 3.2f, false);
        ImGui::SameLine(0.0f, 7.0f);
        if (Theme::fonts().caption)
            ImGui::PushFont(Theme::fonts().caption);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink500));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ok ? T.ink700 : T.ink300));
        ImGui::TextUnformatted(value);
        ImGui::PopStyleColor();
        if (Theme::fonts().caption)
            ImGui::PopFont();
    };
    row(s.pythonOk, "Python", s.pythonOk ? s.pythonVersion.c_str() : TR("common.missing"));
    row(s.venvExists, "venv", s.venvExists ? TR("common.ready") : TR("status.no_venv"));
    row(s.projectReady, TR("status.project"), s.projectReady ? TR("common.ready") : TR("common.missing"));
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    ImGui::Dummy(ImVec2(0, 12.0f));
    if (UI::Button("##exit", TR("nav.exit"), UI::Variant::Ghost, ImVec2(-1, 38),
                   UI::Icon::Power))
        glfwSetWindowShouldClose(g_window, true);

    ImGui::Dummy(ImVec2(0.0f, 0.0f));

    // 右边框
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(wp.x + ws.x - 1.0f, wp.y),
                                              ImVec2(wp.x + ws.x, wp.y + ws.y), T.ink100);
    ImGui::End();
}

// ===========================================================================
// 主内容区
// ===========================================================================
void App::renderMain(const ImVec2 &display)
{
    const Theme::Tokens &T = Theme::T;
    float x = T.sidebarW;
    float w = AcMax(320.0f, display.x - x);
    float h = display.y - T.statusBarH;

    ImGui::SetNextWindowPos(ImVec2(x, 0));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.bg);
    ImGui::Begin("##main", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    const float headerH = 94.0f;
    const float resizerH = 8.0f;

    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    float consoleH = 0.0f;
    if (m_consoleOpen)
        consoleH = AcClamp(h * m_logRatio, 150.0f, AcMax(150.0f, h - 280.0f));
    // 页头、主体、分隔条、控制台之间各有一个 ItemSpacing，必须从主体高度里扣掉
    float bodyH = consoleH > 0.0f
                      ? h - headerH - consoleH - resizerH - spacing * 3.0f
                      : h - headerH - spacing;
    if (bodyH < 130.0f)
    {
        consoleH = 0.0f;
        bodyH = h - headerH - spacing;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(T.pagePad, 16));
    ImGui::BeginChild("##pageheader", ImVec2(0, headerH),
                      ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    renderPageHeader();
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(T.pagePad, 2));
    ImGui::BeginChild("##pagebody", ImVec2(0, bodyH),
                      ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoBackground);
    renderPage();
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::EndChild();
    ImGui::PopStyleVar();

    if (consoleH > 0.0f)
    {
        ImGui::InvisibleButton("##logsplit", ImVec2(ImGui::GetContentRegionAvail().x, resizerH));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemActive())
        {
            float dy = ImGui::GetIO().MouseDelta.y;
            m_logRatio = AcClamp(m_logRatio - dy / AcMax(200.0f, h), 0.18f, 0.62f);
        }
        renderConsolePanel(consoleH);
    }

    // 模态框必须在主窗口的 ID 栈里打开（OpenPopup 与 BeginPopupModal 需同栈）
    renderModals();

    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::End();
}

// ===========================================================================
// 页头
// ===========================================================================
void App::renderPageHeader()
{
    const Theme::Tokens &T = Theme::T;
    struct PageDef
    {
        UI::Icon icon;
        const char *title;
        const char *subtitle;
    };
    const PageDef defs[(int)Page::Count] = {
        {UI::Icon::Grid, "overview.title", "overview.subtitle"},
        {UI::Icon::Cube, "model.title", "model.subtitle"},
        {UI::Icon::Database, "dataset.title", "dataset.subtitle"},
        {UI::Icon::Network, "node.title", "node.subtitle"},
        {UI::Icon::Server, "server.title", "server.subtitle"},
        {UI::Icon::Gear, "env.title", "env.subtitle"},
        {UI::Icon::Sliders, "settings.title", "settings.subtitle"},
    };
    const PageDef &def = defs[(int)m_page];

    ImGui::BeginGroup();
    UI::IconBox(def.icon, 46.0f, UI::Tone::Pink, 0.52f);
    ImGui::SameLine(0.0f, 14.0f);
    ImGui::BeginGroup();
    if (Theme::fonts().display)
        ImGui::PushFont(Theme::fonts().display);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink900));
    ImGui::TextUnformatted(TR(def.title));
    ImGui::PopStyleColor();
    if (Theme::fonts().display)
        ImGui::PopFont();
    if (Theme::fonts().caption)
        ImGui::PushFont(Theme::fonts().caption);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink500));
    ImGui::TextUnformatted(TR(def.subtitle));
    ImGui::PopStyleColor();
    if (Theme::fonts().caption)
        ImGui::PopFont();
    ImGui::EndGroup();
    ImGui::EndGroup();

    // 右侧动作
    std::vector<PageAction> actions;
    AppStateSnapshot s = snapshot();
    switch (m_page)
    {
    case Page::Model:
        actions.push_back({"refresh", TR("common.refresh"), UI::Variant::Ghost, UI::Icon::Refresh,
                           [this]()
                           { refreshModelInfo(); notify(UI::Tone::Info, TR("toast.info_loading")); },
                           s.modelLoading});
        actions.push_back({"open", TR("common.open_dir"), UI::Variant::Soft, UI::Icon::Folder,
                           [this]()
                           { openPath(modelDir()); }});
        break;
    case Page::Dataset:
        actions.push_back({"open", TR("common.open_dir"), UI::Variant::Ghost, UI::Icon::Folder,
                           [this]()
                           { openPath(datasetDir()); }});
        break;
    case Page::Server:
        actions.push_back({"web", TR("server.link_site"), UI::Variant::Soft, UI::Icon::Cloud,
                           [this]()
                           { openUrl(frontendUrl()); }});
        break;
    case Page::Env:
        actions.push_back({"check", TR("common.recheck"), UI::Variant::Ghost, UI::Icon::Refresh,
                           [this]()
                           {
                               const bool ok = checkPython();
                               notify(ok ? UI::Tone::Success : UI::Tone::Danger,
                                      TR(ok ? "env.recheck_ok" : "env.recheck_fail")); }});
        break;
    case Page::Node:
        actions.push_back({"srv", TR("server.link_upload"), UI::Variant::Ghost, UI::Icon::Cloud,
                           [this]()
                           { openUrl(backendUrl() + "/upload"); }});
        break;
    default:
        break;
    }

    if (!actions.empty())
    {
        const float gap = 8.0f;
        const float hgt = 36.0f;
        std::vector<float> widths(actions.size(), 0.0f);
        float total = 0.0f;
        for (size_t i = 0; i < actions.size(); i++)
        {
            float tw = 0.0f;
            if (Theme::fonts().bodyBold)
                ImGui::PushFont(Theme::fonts().bodyBold);
            tw = ImGui::CalcTextSize(actions[i].label).x;
            if (Theme::fonts().bodyBold)
                ImGui::PopFont();
            widths[i] = tw + 36.0f + (actions[i].icon != UI::Icon::None ? 23.0f : 0.0f);
            total += widths[i] + (i ? gap : 0.0f);
        }
        ImGui::SameLine();
        float avail = ImGui::GetContentRegionAvail().x;
        float startX = ImGui::GetCursorScreenPos().x + avail - total;
        float y = ImGui::GetCursorScreenPos().y + (46.0f - hgt) * 0.5f;
        for (size_t i = 0; i < actions.size(); i++)
        {
            ImGui::SetCursorScreenPos(ImVec2(startX, y));
            if (UI::Button(actions[i].id, actions[i].label, actions[i].variant,
                           ImVec2(widths[i], hgt), actions[i].icon, actions[i].disabled))
            {
                if (actions[i].onClick)
                    actions[i].onClick();
            }
            startX += widths[i] + gap;
        }
    }
}

void App::renderPage()
{
    switch (m_page)
    {
    case Page::Overview: renderOverviewPage(); break;
    case Page::Model: renderModelPage(); break;
    case Page::Dataset: renderDatasetPage(); break;
    case Page::Node: renderNodePage(); break;
    case Page::Server: renderServerPage(); break;
    case Page::Env: renderEnvPage(); break;
    case Page::Settings: renderSettingsPage(); break;
    default: break;
    }
}

void App::renderTwoColumn(const std::function<void(float)> &left,
                          const std::function<void(float)> &right)
{
    const Theme::Tokens &T = Theme::T;
    float avail = ImGui::GetContentRegionAvail().x;
    float gap = T.cardGap;
    if (avail < 760.0f)
    {
        left(avail);
        right(avail);
        return;
    }
    float colW = (avail - gap) * 0.5f;
    ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::BeginGroup();
    left(colW);
    ImGui::EndGroup();
    float y1 = ImGui::GetItemRectMax().y;
    ImGui::SameLine(0.0f, gap);
    ImGui::BeginGroup();
    right(colW);
    ImGui::EndGroup();
    float y2 = ImGui::GetItemRectMax().y;
    ImGui::SetCursorScreenPos(ImVec2(start.x, AcMax(y1, y2) + gap));
}

// ===========================================================================
// 控制台面板
// ===========================================================================
void App::renderConsolePanel(float height)
{
    const Theme::Tokens &T = Theme::T;
    const Theme::Fonts &F = Theme::fonts();
    AppStateSnapshot s = snapshot();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(T.pagePad, 0));
    ImGui::BeginChild("##consoleWrap", ImVec2(0, height),
                      ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, T.radiusCard);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.surface);
    ImGui::PushStyleColor(ImGuiCol_Border, T.ink100);
    ImGui::BeginChild("##consoleCard", ImVec2(0, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // ---- 头部 ----
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        UI::DrawIcon(ImGui::GetWindowDrawList(), UI::Icon::Terminal,
                     ImVec2(p.x + 9.0f, p.y + 13.0f), 17.0f, T.pink600);
        ImGui::SetCursorScreenPos(ImVec2(p.x + 24.0f, p.y));
        if (F.heading)
            ImGui::PushFont(F.heading);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink900));
        ImGui::TextUnformatted(TR("console.title"));
        ImGui::PopStyleColor();
        if (F.heading)
            ImGui::PopFont();

        ImGui::SameLine(0.0f, 12.0f);
        if (s.procRunning)
        {
            std::string name = s.procName.empty() ? TR("common.running") : s.procName;
            UI::StatusPill(name.c_str(), UI::Tone::Warning);
            ImGui::SameLine(0.0f, 8.0f);
            if (UI::Button("##stop", TR("common.stop"), UI::Variant::Danger,
                           ImVec2(0, 26), UI::Icon::Stop))
                stopProcess();
        }
        else
        {
            UI::StatusPill(TR("common.idle"), UI::Tone::Neutral);
        }

        // 右侧操作
        const float gap = 6.0f;
        const char *modeLabel = m_consolePlain ? TR("console.colored") : TR("console.plain");
        struct MiniAction
        {
            const char *id;
            const char *label;
            float w;
            std::function<void()> onClick;
        };
        std::vector<MiniAction> mini = {
            {"##mode", modeLabel, 66.0f, [this]()
             { m_consolePlain = !m_consolePlain; }},
            {"##copy", TR("console.copy"), 76.0f, [this]()
             {
                 copyToClipboard(m_console.AllText());
                 notify(UI::Tone::Success, TR("console.copied")); }},
            {"##clear", TR("console.clear"), 52.0f, [this]()
             { m_console.ClearLog(); }},
            {"##hide", TR("console.collapse"), 56.0f, [this]()
             { m_consoleOpen = false; }},
        };
        float total = 0.0f;
        for (size_t i = 0; i < mini.size(); i++)
            total += mini[i].w + (i ? gap : 0.0f);
        float x = p.x + ImGui::GetWindowWidth() - 28.0f - total;
        for (auto &m : mini)
        {
            ImGui::SetCursorScreenPos(ImVec2(x, p.y + 0.0f));
            if (UI::Button(m.id, m.label, UI::Variant::Text, ImVec2(m.w, 27)))
                m.onClick();
            x += m.w + gap;
        }
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 30.0f));
        UI::Divider();
    }

    // ---- 日志 ----
    // 输入行 = 输入框 36 + 与前一项的间距；下缘只保留卡片内边距，避免提示文字被裁切
    const float inputRow = 36.0f + ImGui::GetStyle().ItemSpacing.y;
    ImGui::Dummy(ImVec2(0, 4.0f));
    m_console.DrawLog(ImVec2(0, -inputRow), F.mono, F.caption, m_autoScroll, m_consolePlain);

    m_console.DrawInput([this](const std::string &c)
                        { executeConsoleCommand(c); },
                        TR("console.input_hint"), F.mono, TR("console.send"));

    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ImGui::EndChild();
}

// ===========================================================================
// 状态栏
// ===========================================================================
void App::renderStatusBar(const ImVec2 &display)
{
    const Theme::Tokens &T = Theme::T;
    const Theme::Fonts &F = Theme::fonts();
    AppStateSnapshot s = snapshot();
    float h = T.statusBarH;

    ImGui::SetNextWindowPos(ImVec2(0, display.y - h));
    ImGui::SetNextWindowSize(ImVec2(display.x, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.surface);
    ImGui::Begin("##statusbar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    dl->AddRectFilled(ImVec2(wp.x, wp.y), ImVec2(wp.x + ws.x, wp.y + 1.0f), T.ink100);

    float lineH = ImGui::GetTextLineHeight();
    float yOff = (h - lineH) * 0.5f - 1.0f;
    auto smallText = [&](ImU32 col, const char *text)
    {
        if (F.caption)
            ImGui::PushFont(F.caption);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(col));
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
        if (F.caption)
            ImGui::PopFont();
    };

    // 左侧：统一小圆点样式（无光晕），颜色与侧栏状态卡一致
    ImGui::SetCursorPosY(yOff);
    UI::StatusDot(s.pythonOk ? UI::Tone::Success : UI::Tone::Danger, 3.4f, false);
    ImGui::SameLine(0.0f, 4.0f);
    smallText(T.ink700, TR("status.python"));
    ImGui::SameLine(0.0f, 6.0f);
    smallText(s.pythonOk ? T.ink500 : T.dangerInk,
              s.pythonOk ? s.pythonVersion.c_str() : TR("status.no_python"));

    ImGui::SameLine(0.0f, 16.0f);
    UI::StatusDot(s.venvExists ? UI::Tone::Success : UI::Tone::Danger, 3.4f, false);
    ImGui::SameLine(0.0f, 4.0f);
    smallText(T.ink700, TR("status.venv"));
    ImGui::SameLine(0.0f, 6.0f);
    smallText(T.ink500, s.venvExists ? TR("common.ready") : TR("common.missing"));

    ImGui::SameLine(0.0f, 16.0f);
    smallText(T.ink300, "·");
    ImGui::SameLine(0.0f, 16.0f);

    // 项目路径（裁剪到剩余宽度）
    float rightReserve = 330.0f;
    float pathMax = AcMax(120.0f, ImGui::GetWindowWidth() - ImGui::GetCursorPosX() - rightReserve);
    std::string path = ellipsize(s.projectRoot, F.caption, pathMax);
    smallText(T.ink500, path.c_str());

    // 右侧：控制台开关 + 进程状态（不再显示版本号）
    float x = ws.x - 14.0f;

    // 控制台开关
    const float toggleW = 64.0f, toggleH = 24.0f;
    ImGui::SetCursorScreenPos(ImVec2(wp.x + x - toggleW, wp.y + (h - toggleH) * 0.5f));
    if (UI::Button("##consoleToggle", TR("status.console"), UI::Variant::Text,
                   ImVec2(toggleW, toggleH)))
        m_consoleOpen = !m_consoleOpen;
    x -= toggleW + 12.0f;

    // 进程状态（胶囊以文字行为中心绘制，这里用行高定位避免贴住上边框）
    std::string procLabel = s.procRunning
                                ? (s.procName.empty() ? TR("common.running") : s.procName)
                                : TR("common.idle");
    UI::Tone procTone = s.procRunning ? UI::Tone::Warning : UI::Tone::Neutral;
    if (F.caption)
        ImGui::PushFont(F.caption);
    float pillW = ImGui::CalcTextSize(procLabel.c_str()).x + 26.0f;
    if (F.caption)
        ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(wp.x + x - pillW, wp.y + (h - lineH) * 0.5f));
    UI::StatusPill(procLabel.c_str(), procTone);

    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::End();
}

// ===========================================================================
// 模态框
// ===========================================================================
void App::askConfirm(const std::string &title, const std::string &body,
                     const std::string &okLabel, bool danger, std::function<void()> onOk)
{
    m_confirm.active = true;
    m_confirm.popupOpened = false;
    m_confirm.title = title;
    m_confirm.body = body;
    m_confirm.okLabel = okLabel.empty() ? TR("common.confirm") : okLabel;
    m_confirm.danger = danger;
    m_confirm.onOk = std::move(onOk);
    markDirty();
}

void App::askInput(const std::string &title, const std::string &label,
                   const std::string &value, bool password, const std::string &okLabel,
                   std::function<void(const std::string &)> onOk)
{
    m_input.active = true;
    m_input.popupOpened = false;
    m_input.focusPending = true;
    m_input.title = title;
    m_input.label = label;
    m_input.okLabel = okLabel.empty() ? TR("common.confirm") : okLabel;
    m_input.password = password;
    m_input.onOk = std::move(onOk);
    strncpy_s(m_input.value, value.c_str(), sizeof(m_input.value) - 1);
    markDirty();
}

void App::renderModals()
{
    const Theme::Tokens &T = Theme::T;

    // ------------------------------ 确认框 ------------------------------
    if (m_confirm.active)
    {
        if (!m_confirm.popupOpened)
        {
            ImGui::OpenPopup("##confirm_modal");
            m_confirm.popupOpened = true;
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 22));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, T.radiusModal);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, T.surface);
        ImGui::SetNextWindowSizeConstraints(ImVec2(380, 0), ImVec2(520, FLT_MAX));
        bool open = ImGui::BeginPopupModal("##confirm_modal", nullptr,
                                           ImGuiWindowFlags_AlwaysAutoResize |
                                               ImGuiWindowFlags_NoTitleBar |
                                               ImGuiWindowFlags_NoSavedSettings);

        if (open)
        {
            UI::IconBox(m_confirm.danger ? UI::Icon::Trash : UI::Icon::Info, 40.0f,
                        m_confirm.danger ? UI::Tone::Danger : UI::Tone::Pink, 0.5f);
            ImGui::SameLine(0.0f, 12.0f);
            if (Theme::fonts().heading)
                ImGui::PushFont(Theme::fonts().heading);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink900));
            ImGui::TextUnformatted(m_confirm.title.c_str());
            ImGui::PopStyleColor();
            if (Theme::fonts().heading)
                ImGui::PopFont();

            ImGui::Dummy(ImVec2(0, 10.0f));
            ImGui::PushTextWrapPos(440.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink700));
            ImGui::TextUnformatted(m_confirm.body.c_str());
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
            ImGui::Dummy(ImVec2(0, 14.0f));

            const float bwCancel = 88.0f, bwOk = 104.0f, gap = 10.0f;
            float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + AcMax(0.0f, avail - bwCancel - bwOk - gap));
            bool cancelled = UI::Button("##m_cancel", TR("common.cancel"), UI::Variant::Ghost,
                                        ImVec2(bwCancel, 38));
            ImGui::SameLine(0.0f, gap);
            bool confirmed = UI::Button("##m_ok", m_confirm.okLabel.c_str(),
                                        m_confirm.danger ? UI::Variant::Danger : UI::Variant::Solid,
                                        ImVec2(bwOk, 38));
            if (cancelled)
            {
                m_confirm.active = false;
                m_confirm.onOk = nullptr;
                ImGui::CloseCurrentPopup();
            }
            else if (confirmed)
            {
                auto fn = m_confirm.onOk;
                m_confirm.active = false;
                m_confirm.onOk = nullptr;
                ImGui::CloseCurrentPopup();
                if (fn)
                    fn();
            }
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        if (!ImGui::IsPopupOpen("##confirm_modal") && m_confirm.active)
        {
            m_confirm.active = false;
            m_confirm.onOk = nullptr;
        }
    }

    // ------------------------------ 输入框 ------------------------------
    if (m_input.active)
    {
        if (!m_input.popupOpened)
        {
            ImGui::OpenPopup("##input_modal");
            m_input.popupOpened = true;
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 22));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, T.radiusModal);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, T.surface);
        ImGui::SetNextWindowSizeConstraints(ImVec2(420, 0), ImVec2(560, FLT_MAX));
        bool open = ImGui::BeginPopupModal("##input_modal", nullptr,
                                           ImGuiWindowFlags_AlwaysAutoResize |
                                               ImGuiWindowFlags_NoTitleBar |
                                               ImGuiWindowFlags_NoSavedSettings);
        if (open)
        {
            UI::IconBox(UI::Icon::Sparkle, 40.0f, UI::Tone::Pink, 0.5f);
            ImGui::SameLine(0.0f, 12.0f);
            if (Theme::fonts().heading)
                ImGui::PushFont(Theme::fonts().heading);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink900));
            ImGui::TextUnformatted(m_input.title.c_str());
            ImGui::PopStyleColor();
            if (Theme::fonts().heading)
                ImGui::PopFont();

            ImGui::Dummy(ImVec2(0, 10.0f));
            if (Theme::fonts().caption)
                ImGui::PushFont(Theme::fonts().caption);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink500));
            ImGui::TextUnformatted(m_input.label.c_str());
            ImGui::PopStyleColor();
            if (Theme::fonts().caption)
                ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, 2.0f));

            if (m_input.focusPending)
            {
                ImGui::SetKeyboardFocusHere();
                m_input.focusPending = false;
            }
            ImGui::SetNextItemWidth(-1);
            ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
            if (m_input.password)
                flags |= ImGuiInputTextFlags_Password;
            bool submit = ImGui::InputText("##input_value", m_input.value, sizeof(m_input.value), flags);

            ImGui::Dummy(ImVec2(0, 14.0f));
            const float bwCancel = 88.0f, bwOk = 104.0f, gap = 10.0f;
            float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + AcMax(0.0f, avail - bwCancel - bwOk - gap));
            bool cancelled = UI::Button("##i_cancel", TR("common.cancel"), UI::Variant::Ghost,
                                        ImVec2(bwCancel, 38));
            ImGui::SameLine(0.0f, gap);
            bool confirmed = UI::Button("##i_ok", m_input.okLabel.c_str(), UI::Variant::Solid,
                                        ImVec2(bwOk, 38));
            if (submit)
                confirmed = true;

            if (cancelled)
            {
                m_input.active = false;
                m_input.onOk = nullptr;
                ImGui::CloseCurrentPopup();
            }
            else if (confirmed)
            {
                std::string value(m_input.value);
                auto fn = m_input.onOk;
                m_input.active = false;
                m_input.onOk = nullptr;
                ImGui::CloseCurrentPopup();
                if (fn)
                    fn(value);
            }
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        if (!ImGui::IsPopupOpen("##input_modal") && m_input.active)
        {
            m_input.active = false;
            m_input.onOk = nullptr;
        }
    }
}
