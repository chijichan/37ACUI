#include "tray.h"

#include "lang.h"

#include <string>

#define WM_TRAYICON (WM_APP + 101)

namespace
{
const wchar_t *kClassName = L"37ACUI.TrayHost";
constexpr UINT kCmdShow = 1;
constexpr UINT kCmdQuit = 2;

UINT g_taskbarCreatedMsg = 0;

std::wstring widen(const char *utf8)
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
} // namespace

TrayIcon::~TrayIcon() { destroy(); }

bool TrayIcon::create(Callback onShow, Callback onQuit)
{
    m_onShow = std::move(onShow);
    m_onQuit = std::move(onQuit);

    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &TrayIcon::WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc); // 已注册时返回 0，忽略

    // 一个隐藏的顶层窗口（而不是 HWND_MESSAGE）：这样 SetForegroundWindow/TrackPopupMenu
    // 的行为与普通菜单一致，点击菜单外部能正常收起。
    m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kClassName, L"37ACUI Tray", WS_POPUP,
                             0, 0, 0, 0, nullptr, nullptr, instance, this);
    if (!m_hwnd)
        return false;

    g_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
    loadIcon();
    return true;
}

void TrayIcon::loadIcon()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    const int cx = GetSystemMetrics(SM_CXSMICON);
    const int cy = GetSystemMetrics(SM_CYSMICON);
    m_icon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON, cx, cy, 0);
    m_iconOwned = (m_icon != nullptr);
    if (!m_icon)
        m_icon = LoadIconW(instance, MAKEINTRESOURCEW(101)); // 共享图标，不销毁
}

void TrayIcon::show(const wchar_t *balloonTitle, const wchar_t *balloonText)
{
    if (!m_hwnd)
        return;

    m_nid = {};
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = m_icon;

    std::wstring tip = widen(TR("tray.tip"));
    wcsncpy_s(m_nid.szTip, tip.empty() ? L"37ACUI" : tip.c_str(), _TRUNCATE);

    const bool withBalloon = balloonText && balloonText[0] && !m_balloonShown;
    if (withBalloon)
    {
        m_nid.uFlags |= NIF_INFO;
        wcsncpy_s(m_nid.szInfo, balloonText, _TRUNCATE);
        wcsncpy_s(m_nid.szInfoTitle,
                  (balloonTitle && balloonTitle[0]) ? balloonTitle : L"37ACUI", _TRUNCATE);
        m_nid.dwInfoFlags = NIIF_INFO;
    }

    BOOL ok = Shell_NotifyIconW(NIM_ADD, &m_nid);
    if (!ok)
        ok = Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    m_added = (ok != FALSE);
    if (withBalloon && ok)
        m_balloonShown = true;
}

void TrayIcon::hide()
{
    if (!m_added || !m_hwnd)
        return;
    Shell_NotifyIconW(NIM_DELETE, &m_nid);
    m_added = false;
}

void TrayIcon::destroy()
{
    hide();
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_iconOwned && m_icon)
    {
        DestroyIcon(m_icon);
        m_icon = nullptr;
        m_iconOwned = false;
    }
}

void TrayIcon::showMenu()
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    std::wstring showText = widen(TR("tray.show"));
    std::wstring quitText = widen(TR("tray.quit"));
    AppendMenuW(menu, MF_STRING, kCmdShow, showText.empty() ? L"Show" : showText.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdQuit, quitText.empty() ? L"Exit" : quitText.c_str());

    POINT pt = {};
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0,
                   m_hwnd, nullptr);
    DestroyMenu(menu);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
}

LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    TrayIcon *self = (TrayIcon *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (self)
        return self->handleMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT TrayIcon::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Explorer 重启后会丢失托盘图标，收到 TaskbarCreated 时重新注册
    if (g_taskbarCreatedMsg != 0 && msg == g_taskbarCreatedMsg)
    {
        if (m_added)
        {
            m_added = false;
            show();
        }
        return 0;
    }

    switch (msg)
    {
    case WM_TRAYICON:
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            if (m_onShow)
                m_onShow();
            return 0;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            showMenu();
            return 0;
        default:
            break;
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case kCmdShow:
            if (m_onShow)
                m_onShow();
            return 0;
        case kCmdQuit:
            if (m_onQuit)
                m_onQuit();
            return 0;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
