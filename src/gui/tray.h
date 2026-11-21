#pragma once

// ============================================================================
// 系统托盘图标（最小化到托盘时使用）
//
// 用一个独立的 message-only 窗口接收 Shell_NotifyIcon 的回调，
// 因此不依赖 GLFW 窗口是否存在，也不干扰 ImGui 的消息处理。
// ============================================================================

#include <windows.h>
#include <shellapi.h>

#include <functional>

class TrayIcon
{
public:
    using Callback = std::function<void()>;

    TrayIcon() = default;
    ~TrayIcon();

    TrayIcon(const TrayIcon &) = delete;
    TrayIcon &operator=(const TrayIcon &) = delete;

    // 创建托盘宿主窗口（不显示图标）。icon 由 tray 自己按资源 ID 101 加载。
    bool create(Callback onShow, Callback onQuit);

    void destroy();

    // 显示 / 隐藏托盘图标；首次显示可附带气泡提示
    void show(const wchar_t *balloonTitle = nullptr, const wchar_t *balloonText = nullptr);
    void hide();
    bool visible() const { return m_added; }

    HWND hwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void showMenu();
    void loadIcon();

    HWND m_hwnd = nullptr;
    HICON m_icon = nullptr;
    bool m_iconOwned = false;
    bool m_added = false;
    bool m_balloonShown = false;
    NOTIFYICONDATAW m_nid = {};
    Callback m_onShow;
    Callback m_onQuit;
};
