#include "console.h"
#include <cstring>
#include <cstdio>

ConsoleWidget::ConsoleWidget()
{
    m_historyPos = -1;
    memset(m_inputBuf, 0, sizeof(m_inputBuf));
}

ConsoleWidget::~ConsoleWidget()
{
    for (int i = 0; i < m_history.Size; i++)
        ImGui::MemFree(m_history[i]);
}

void ConsoleWidget::Strtrim(char *s)
{
    char *end = s + strlen(s);
    while (end > s && end[-1] == ' ')
        end--;
    *end = 0;
}

char *ConsoleWidget::Strdup(const char *s)
{
    IM_ASSERT(s);
    size_t len = strlen(s) + 1;
    void *buf = ImGui::MemAlloc(len);
    IM_ASSERT(buf);
    return (char *)memcpy(buf, (const void *)s, len);
}

void ConsoleWidget::ClearLog()
{
    m_fullText.clear();
}

void ConsoleWidget::AddLog(const std::string &line)
{
    m_fullText += line + "\n";
    if (m_fullText.size() > 500 * 1024)
    {
        auto pos = m_fullText.find('\n', m_fullText.size() / 2);
        if (pos != std::string::npos)
            m_fullText.erase(0, pos + 1);
    }
}

void ConsoleWidget::Draw(const char *title, ImVec2 size, bool hasInput,
                         std::function<void(const std::string &)> onCommand,
                         const char *hint, ImFont *monoFont,
                         const char *statusText, ImU32 statusColor,
                         std::function<void()> onStop, const char *stopLabel)
{
    // 深色终端配色
    const ImVec4 darkBg(0.105f, 0.115f, 0.135f, 1.00f);
    const ImVec4 darkBgActive(0.135f, 0.145f, 0.165f, 1.00f);
    const ImVec4 lightText(0.88f, 0.91f, 0.95f, 1.00f);

    // 用 BeginChild 包裹确保 InputTextMultiline 占满可用区域
    ImGui::BeginChild("ConsoleOut", size, false, ImGuiWindowFlags_HorizontalScrollbar);

    // 输出区域 — InputTextMultiline 只读模式，原生支持框选和 Ctrl+C
    ImGui::PushStyleColor(ImGuiCol_FrameBg, darkBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, darkBgActive);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, darkBgActive);
    ImGui::PushStyleColor(ImGuiCol_Text, lightText);
    if (monoFont)
        ImGui::PushFont(monoFont); // 等宽字体，更接近专业终端

    static char consoleBuf[65536 * 4];
    strncpy_s(consoleBuf, m_fullText.c_str(), sizeof(consoleBuf) - 1);
    consoleBuf[sizeof(consoleBuf) - 1] = '\0';

    ImGui::InputTextMultiline("##console_out", consoleBuf, sizeof(consoleBuf),
                              ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);

    if (monoFont)
        ImGui::PopFont();
    ImGui::PopStyleColor(4);

    ImGui::EndChild();

    // 输入框
    if (hasInput && onCommand)
    {
        ImGui::Separator();
        bool reclaim = m_reclaimFocus;
        m_reclaimFocus = false;

        ImGui::PushItemWidth(-60);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, darkBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, darkBgActive);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, darkBgActive);
        ImGui::PushStyleColor(ImGuiCol_Text, lightText);
        if (monoFont)
            ImGui::PushFont(monoFont);
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;
        if (ImGui::InputText("##cmd", m_inputBuf, sizeof(m_inputBuf),
                             flags, &TextEditCallbackStub, (void *)this))
        {
            char *s = m_inputBuf;
            Strtrim(s);
            if (s[0])
            {
                AddLog(std::string("> ") + s);
                if (onCommand)
                    onCommand(std::string(s));
            }
            s[0] = '\0';
            reclaim = true;
        }
        if (monoFont)
            ImGui::PopFont();
        ImGui::PopStyleColor(4);
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button("发送", ImVec2(50, 0)))
        {
            char *s = m_inputBuf;
            Strtrim(s);
            if (s[0])
            {
                AddLog(std::string("> ") + s);
                if (onCommand)
                    onCommand(std::string(s));
            }
            s[0] = '\0';
            reclaim = true;
        }

        // 进程状态 + 停止按钮 + 指令提示（显示在输入框下方）
        bool hasStatus = statusText && statusText[0] && statusColor != 0;
        if (hasStatus)
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(statusColor), "%s", statusText);
            // 停止按钮紧随状态文本（仅在有回调时显示）
            if (onStop && stopLabel && stopLabel[0])
            {
                ImGui::SameLine();
                if (ImGui::SmallButton(stopLabel))
                    onStop();
            }
        }
        if (hint)
        {
            if (hasStatus)
                ImGui::SameLine();
            ImGui::TextDisabled("%s", hint);
        }

        if (reclaim)
            ImGui::SetKeyboardFocusHere(-1);
    }
}

int ConsoleWidget::TextEditCallbackStub(ImGuiInputTextCallbackData *data)
{
    ConsoleWidget *console = (ConsoleWidget *)data->UserData;
    return console->TextEditCallback(data);
}

int ConsoleWidget::TextEditCallback(ImGuiInputTextCallbackData *data)
{
    switch (data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackHistory:
    {
        const int prevPos = m_historyPos;
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (m_historyPos == -1)
                m_historyPos = m_history.Size - 1;
            else if (m_historyPos > 0)
                m_historyPos--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (m_historyPos != -1)
                if (++m_historyPos >= m_history.Size)
                    m_historyPos = -1;
        }
        if (prevPos != m_historyPos)
        {
            const char *histStr = (m_historyPos >= 0) ? m_history[m_historyPos] : "";
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, histStr);
        }
        break;
    }
    }
    return 0;
}