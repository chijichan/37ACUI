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

// 按显示宽度对文本做软换行：ImGui 的 InputTextMultiline 不自动换行（AddText wrap_width=0），
// 超宽行会水平延伸。这里在重建显示缓冲时按字符 advance 累加宽度，超宽处插入 '\n'。
void ConsoleWidget::RebuildDisplay(const std::string &text, float wrapWidth)
{
    m_displayBuf.clear();
    m_displayBuf.reserve(text.size() + text.size() / 80 + 64);

    // ImGui 1.92+: 字符宽度查询移到 ImFontBaked（GetCharAdvance 返回实际像素宽度，已含缩放）
    ImFont *font = const_cast<ImFont *>(ImGui::GetFont());
    ImFontBaked *baked = font->GetFontBaked(ImGui::GetFontSize());
    float x = 0.0f;
    const char *p = text.c_str();
    const char *end = p + text.size();
    while (p < end)
    {
        unsigned int c = (unsigned char)*p;
        const char *next = p + 1;
        // 手动解码 UTF-8（避免依赖 imgui_internal.h）
        if ((c & 0x80) == 0)
        {
            c = (unsigned char)*p;
        }
        else if ((c & 0xE0) == 0xC0 && p + 1 < end && ((unsigned char)p[1] & 0xC0) == 0x80)
        {
            c = ((c & 0x1F) << 6) | ((unsigned char)p[1] & 0x3F);
            next = p + 2;
        }
        else if ((c & 0xF0) == 0xE0 && p + 2 < end && ((unsigned char)p[1] & 0xC0) == 0x80 && ((unsigned char)p[2] & 0xC0) == 0x80)
        {
            c = ((c & 0x0F) << 12) | (((unsigned char)p[1] & 0x3F) << 6) | ((unsigned char)p[2] & 0x3F);
            next = p + 3;
        }
        else if ((c & 0xF8) == 0xF0 && p + 3 < end && ((unsigned char)p[1] & 0xC0) == 0x80 && ((unsigned char)p[2] & 0xC0) == 0x80 && ((unsigned char)p[3] & 0xC0) == 0x80)
        {
            c = ((c & 0x07) << 18) | (((unsigned char)p[1] & 0x3F) << 12) | (((unsigned char)p[2] & 0x3F) << 6) | ((unsigned char)p[3] & 0x3F);
            next = p + 4;
        }

        if (c == '\n')
        {
            m_displayBuf += '\n';
            x = 0.0f;
            p = next;
            continue;
        }
        if (c == '\r')
        {
            p = next;
            continue;
        }

        const float w = baked->GetCharAdvance((ImWchar)c);
        if (x > 0.0f && x + w > wrapWidth)
        {
            m_displayBuf += '\n'; // 软换行：仅显示层插入，不改动原始日志
            x = 0.0f;
        }
        m_displayBuf.append(p, next - p);
        x += w;
        p = next;
    }
}

void ConsoleWidget::Draw(const char *title, ImVec2 size, bool hasInput,
                         std::function<void(const std::string &)> onCommand,
                         const char *hint, ImFont *monoFont,
                         const char *statusText, ImU32 statusColor,
                         std::function<void()> onStop, const char *stopLabel,
                         bool autoScroll)
{
    // 深色终端配色
    const ImVec4 darkBg(0.105f, 0.115f, 0.135f, 1.00f);
    const ImVec4 darkBgActive(0.135f, 0.145f, 0.165f, 1.00f);
    const ImVec4 lightText(0.88f, 0.91f, 0.95f, 1.00f);

    // 用 BeginChild 包裹确保 InputTextMultiline 占满可用区域
    // 注意：不能加 ImGuiWindowFlags_HorizontalScrollbar——水平滚动会禁用换行，
    // 超宽行将延伸到窗口外。换行由 RebuildDisplay 的软换行实现。
    ImGui::BeginChild("ConsoleOut", size, false);

    // 输出区域 — InputTextMultiline 只读模式，原生支持框选和 Ctrl+C
    ImGui::PushStyleColor(ImGuiCol_FrameBg, darkBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, darkBgActive);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, darkBgActive);
    ImGui::PushStyleColor(ImGuiCol_Text, lightText);
    if (monoFont)
        ImGui::PushFont(monoFont); // 等宽字体，更接近专业终端

    // 仅当内容变化或窗口宽度变化时重建显示缓冲（避免每帧重建，也避免每帧 strncpy）
    // 保留最新 256KB 原始文本，软换行后超宽行折行显示。
    const bool contentChanged = (m_fullText.size() != m_displaySize);
    const float availW = ImGui::GetContentRegionAvail().x;
    const float wrapW = availW - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetStyle().ScrollbarSize;
    if (contentChanged || wrapW != m_wrapWidth)
    {
        m_displaySize = m_fullText.size();
        m_wrapWidth = wrapW;
        std::string src = m_fullText;
        if (src.size() > kMaxDisplayBytes)
            src.erase(0, src.size() - kMaxDisplayBytes); // 保留最新部分
        RebuildDisplay(src, wrapW);
    }

    // 自动滚动：内容新增时让 InputTextMultiline 内部滚动区跳到最底部（最新日志）
    // SetNextWindowScroll 作用于下一个窗口，即 InputTextMultiline 内部创建的 child。
    if (autoScroll && contentChanged)
        ImGui::SetNextWindowScroll(ImVec2(0.0f, 1e9f));

    ImGui::InputTextMultiline("##console_out", &m_displayBuf[0], (int)m_displayBuf.size() + 1,
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