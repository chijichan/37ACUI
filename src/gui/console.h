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
              const char *hint = nullptr, ImFont *monoFont = nullptr);

    void FocusInput() { m_reclaimFocus = true; }

private:
    std::string m_fullText; // 全部文本（InputTextMultiline 显示用）
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