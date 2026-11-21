#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "imgui.h"

// ============================================================================
// 控制台
// 彩色模式：按级别着色的行列表（虚拟化渲染，支持海量日志）
// 纯文本模式：只读多行文本，支持鼠标框选与 Ctrl+C
// ============================================================================
class ConsoleWidget
{
public:
    enum class Level
    {
        Info,
        Success,
        Warn,
        Error,
        Command
    };

    ConsoleWidget();
    ~ConsoleWidget();

    void ClearLog();
    void AddLine(Level level, const std::string &text);
    void AddLog(const std::string &text) { AddLine(Level::Info, text); }

    // 日志区（size 为可用区域，内部自带滚动条）
    void DrawLog(ImVec2 size, ImFont *monoFont, ImFont *smallFont,
                 bool autoScroll, bool plainMode);

    // 输入行（含历史记录、发送按钮）。hint 显示在输入框内。
    bool DrawInput(std::function<void(const std::string &)> onCommand,
                   const char *hint, ImFont *monoFont, const char *sendLabel);

    // 导出全部日志（用于「复制全部」）
    std::string AllText();

    // 最近 n 行（概览页的「最近日志」）
    struct TailLine
    {
        std::string time;
        Level level;
        std::string text;
    };
    std::vector<TailLine> Tail(size_t n);

    size_t lineCount();

    // 归还日志缓冲的多余容量（最小化到托盘时调用）
    void CompactMemory();

    void FocusInput() { m_reclaimFocus = true; }

private:
    struct Line
    {
        std::string time;
        Level level;
        std::string text;
    };

    void rebuildPlain();
    void rebuildWrapped(float wrapWidth);

    mutable std::mutex m_mutex;
    std::vector<Line> m_lines;
    size_t m_plainLineCount = (size_t)-1;

    std::string m_plainText;    // 纯文本模式原始内容
    std::string m_wrappedText;  // 纯文本模式软换行后的显示缓冲
    float m_wrapWidth = 0.0f;

    bool m_scrollToBottom = true;
    bool m_wasPlain = false;

    std::vector<std::string> m_history;
    int m_historyPos = -1;
    char m_inputBuf[512] = "";
    bool m_reclaimFocus = false;

    static int TextEditCallbackStub(ImGuiInputTextCallbackData *data);
    int TextEditCallback(ImGuiInputTextCallbackData *data);
    static void Strtrim(char *s);
};
