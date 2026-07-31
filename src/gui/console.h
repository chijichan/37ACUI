#pragma once

#include <string>
#include <vector>
#include <functional>
#include "imgui.h"

class ConsoleWidget
{
public:
    ConsoleWidget();
    ~ConsoleWidget();

    void ClearLog();
    void AddLog(const std::string &line);
    void Draw(const char *title, ImVec2 size, bool hasInput = false,
              std::function<void(const std::string &)> onCommand = nullptr,
              const char *hint = nullptr, ImFont *monoFont = nullptr,
              const char *statusText = nullptr, ImU32 statusColor = 0,
              std::function<void()> onStop = nullptr, const char *stopLabel = nullptr);

    void FocusInput() { m_reclaimFocus = true; }

private:
    std::string m_fullText; // 全部文本（InputTextMultiline 显示用）
    std::string m_displayBuf;            // 显示缓冲：仅内容变化时从 m_fullText 刷新，避免每帧拷贝
    size_t m_displaySize = (size_t)-1;   // 上次刷新时的 m_fullText 长度
    static constexpr size_t kMaxDisplayBytes = 65536 * 4; // 显示缓冲上限
    ImVector<char *> m_history;
    int m_historyPos = -1;
    char m_inputBuf[256] = "";
    bool m_reclaimFocus = false;

    void ExecCommand(const std::string &cmd,
                     std::function<void(const std::string &)> onCommand);

    static int TextEditCallbackStub(ImGuiInputTextCallbackData *data);
    int TextEditCallback(ImGuiInputTextCallbackData *data);

    static void Strtrim(char *s);
    static char *Strdup(const char *s);
};