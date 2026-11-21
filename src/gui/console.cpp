#include "console.h"

#include "components.h"
#include "theme.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace
{
constexpr size_t kMaxLines = 4000;
constexpr size_t kTrimTo = 3000;
constexpr float kRowHeight = 21.0f;

struct LevelStyle
{
    ImU32 text;
    ImU32 dot;
};

LevelStyle styleFor(ConsoleWidget::Level level)
{
    const Theme::Tokens &T = Theme::T;
    switch (level)
    {
    case ConsoleWidget::Level::Success:
        return {T.successInk, T.success};
    case ConsoleWidget::Level::Warn:
        return {T.warningInk, T.warning};
    case ConsoleWidget::Level::Error:
        return {T.dangerInk, T.danger};
    case ConsoleWidget::Level::Command:
        return {T.pink700, T.pink400};
    default:
        return {T.ink700, T.ink300};
    }
}

void timestamp(char *buf, size_t n)
{
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    struct tm tm;
    localtime_s(&tm, &tt);
    strftime(buf, n, "%H:%M:%S", &tm);
}

// 从日志正文推断级别（37AC 的 Python 日志带 [INFO]/[ERROR] 前缀）
ConsoleWidget::Level detectLevel(const std::string &text, ConsoleWidget::Level given)
{
    if (given != ConsoleWidget::Level::Info)
        return given;
    if (text.find("[ERROR]") != std::string::npos || text.find("Traceback") != std::string::npos ||
        text.find("错误") != std::string::npos || text.find("失败") != std::string::npos ||
        text.find("Exception") != std::string::npos)
        return ConsoleWidget::Level::Error;
    if (text.find("[WARNING]") != std::string::npos || text.find("[warn]") != std::string::npos ||
        text.find("警告") != std::string::npos)
        return ConsoleWidget::Level::Warn;
    if (text.find("[SUCCESS]") != std::string::npos || text.find("完成") != std::string::npos ||
        text.find("成功") != std::string::npos)
        return ConsoleWidget::Level::Success;
    return given;
}
} // namespace

ConsoleWidget::ConsoleWidget()
{
    m_historyPos = -1;
    memset(m_inputBuf, 0, sizeof(m_inputBuf));
}

ConsoleWidget::~ConsoleWidget() = default;

void ConsoleWidget::Strtrim(char *s)
{
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t'))
        end--;
    *end = 0;
}

void ConsoleWidget::ClearLog()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lines.clear();
    m_lines.shrink_to_fit();
    m_plainText.clear();
    m_plainText.shrink_to_fit();
    m_wrappedText.clear();
    m_wrappedText.shrink_to_fit();
    m_plainLineCount = (size_t)-1;
    m_wrapWidth = 0.0f;
    m_scrollToBottom = true;
}

void ConsoleWidget::AddLine(Level level, const std::string &text)
{
    char ts[16] = {0};
    timestamp(ts, sizeof(ts));

    std::lock_guard<std::mutex> lock(m_mutex);
    Line line;
    line.time = ts;
    line.text = text;
    line.level = detectLevel(text, level);
    m_lines.push_back(std::move(line));

    // 命令历史（上下键回溯）
    if (level == Level::Command)
    {
        std::string cmd = text;
        if (!cmd.empty() && cmd[0] == '>')
            cmd.erase(0, cmd.find_first_not_of("> "));
        if (!cmd.empty())
        {
            m_history.push_back(cmd);
            if (m_history.size() > 64)
                m_history.erase(m_history.begin());
        }
    }

    if (m_lines.size() > kMaxLines)
        m_lines.erase(m_lines.begin(), m_lines.begin() + (m_lines.size() - kTrimTo));

    m_plainLineCount = (size_t)-1; // 纯文本缓冲失效
}

size_t ConsoleWidget::lineCount()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lines.size();
}

std::string ConsoleWidget::AllText()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string out;
    out.reserve(m_plainText.size() + 64);
    for (const Line &l : m_lines)
    {
        out += "[";
        out += l.time;
        out += "] ";
        out += l.text;
        out += '\n';
    }
    return out;
}

void ConsoleWidget::CompactMemory()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lines.shrink_to_fit();
    for (Line &l : m_lines)
    {
        l.time.shrink_to_fit();
        l.text.shrink_to_fit();
    }
    // 纯文本 / 软换行缓存都是可重建的派生数据，直接丢弃
    m_plainText.clear();
    m_plainText.shrink_to_fit();
    m_wrappedText.clear();
    m_wrappedText.shrink_to_fit();
    m_plainLineCount = (size_t)-1;
    m_wrapWidth = 0.0f;
}

std::vector<ConsoleWidget::TailLine> ConsoleWidget::Tail(size_t n)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<TailLine> out;
    size_t start = m_lines.size() > n ? m_lines.size() - n : 0;
    out.reserve(m_lines.size() - start);
    for (size_t i = start; i < m_lines.size(); i++)
        out.push_back({m_lines[i].time, m_lines[i].level, m_lines[i].text});
    return out;
}

void ConsoleWidget::rebuildPlain()
{
    m_plainText.clear();
    m_plainText.reserve(m_lines.size() * 96);
    for (const Line &l : m_lines)
    {
        m_plainText += "[";
        m_plainText += l.time;
        m_plainText += "] ";
        m_plainText += l.text;
        m_plainText += '\n';
    }
    m_plainLineCount = m_lines.size();
    m_wrapWidth = 0.0f;
}

// 纯文本模式按窗口宽度做软换行（ImGui 的只读多行文本不会自动折行）
void ConsoleWidget::rebuildWrapped(float wrapWidth)
{
    m_wrapWidth = wrapWidth;
    m_wrappedText.clear();
    m_wrappedText.reserve(m_plainText.size() + m_plainText.size() / 40 + 64);

    ImFont *font = ImGui::GetFont();
    ImFontBaked *baked = font ? font->GetFontBaked(ImGui::GetFontSize()) : nullptr;
    if (!baked)
    {
        m_wrappedText = m_plainText;
        return;
    }

    float x = 0.0f;
    const char *p = m_plainText.c_str();
    const char *end = p + m_plainText.size();
    while (p < end)
    {
        unsigned int c = (unsigned char)*p;
        const char *next = p + 1;
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
            m_wrappedText += '\n';
            x = 0.0f;
            p = next;
            continue;
        }
        if (c == '\r')
        {
            p = next;
            continue;
        }

        float w = baked->GetCharAdvance((ImWchar)c);
        if (w <= 0.0f)
            w = baked->GetCharAdvance((ImWchar)'?');
        if (x > 0.0f && x + w > wrapWidth)
        {
            m_wrappedText += '\n';
            x = 0.0f;
        }
        m_wrappedText.append(p, next - p);
        x += w;
        p = next;
    }
}

void ConsoleWidget::DrawLog(ImVec2 size, ImFont *monoFont, ImFont *smallFont,
                            bool autoScroll, bool plainMode)
{
    const Theme::Tokens &T = Theme::T;

    ImGui::BeginChild("##console_lines", size, false,
                      plainMode ? ImGuiWindowFlags_None : ImGuiWindowFlags_HorizontalScrollbar);

    // ---------------- 纯文本模式（可框选复制） ----------------
    if (plainMode)
    {
        std::string plain;
        bool contentChanged = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_plainLineCount != m_lines.size())
            {
                rebuildPlain();
                contentChanged = true;
            }
            plain = m_plainText;
        }
        float availW = ImGui::GetContentRegionAvail().x;
        float wrapW = availW - ImGui::GetStyle().ScrollbarSize - 8.0f;
        if (wrapW < 40.0f)
            wrapW = 40.0f;
        if (contentChanged || wrapW != m_wrapWidth)
            rebuildWrapped(wrapW);

        if (m_wrappedText.empty())
            m_wrappedText = " ";

        if (monoFont)
            ImGui::PushFont(monoFont);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, T.surface);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink700));
        if (autoScroll && contentChanged)
            ImGui::SetNextWindowScroll(ImVec2(0.0f, 1e9f));
        ImGui::InputTextMultiline("##console_plain", &m_wrappedText[0],
                                  (int)m_wrappedText.size() + 1, ImVec2(-1, -1),
                                  ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor(2);
        if (monoFont)
            ImGui::PopFont();

        ImGui::EndChild();
        m_wasPlain = true;
        return;
    }
    m_wasPlain = false;

    // ---------------- 彩色模式（按级别着色 + 虚拟化） ----------------
    ImDrawList *dl = ImGui::GetWindowDrawList();
    bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 6.0f;
    if (ImGui::GetScrollMaxY() <= 0.0f)
        atBottom = true;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const int count = (int)m_lines.size();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

        ImGuiListClipper clipper;
        clipper.Begin(count, kRowHeight);
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                const Line &line = m_lines[(size_t)i];
                LevelStyle st = styleFor(line.level);
                ImVec2 rp = ImGui::GetCursorScreenPos();

                dl->AddCircleFilled(ImVec2(rp.x + 6.0f, rp.y + 9.0f), 2.8f, st.dot);

                float x0 = ImGui::GetCursorPosX();
                ImGui::SetCursorPosX(x0 + 15.0f);
                if (smallFont)
                    ImGui::PushFont(smallFont);
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink300));
                ImGui::TextUnformatted(line.time.c_str());
                ImGui::PopStyleColor();
                if (smallFont)
                    ImGui::PopFont();

                ImGui::SameLine(0.0f, 8.0f);
                if (monoFont)
                    ImGui::PushFont(monoFont);
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(st.text));
                ImGui::TextUnformatted(line.text.c_str());
                ImGui::PopStyleColor();
                if (monoFont)
                    ImGui::PopFont();

                ImGui::Dummy(ImVec2(0.0f, kRowHeight - ImGui::GetTextLineHeight()));
            }
        }
        ImGui::PopStyleVar();
    }

    if (autoScroll && (atBottom || m_scrollToBottom))
        ImGui::SetScrollHereY(1.0f);
    m_scrollToBottom = false;

    ImGui::EndChild();
}

bool ConsoleWidget::DrawInput(std::function<void(const std::string &)> onCommand,
                              const char *hint, ImFont *monoFont, const char *sendLabel)
{
    bool submitted = false;
    bool reclaim = m_reclaimFocus;
    m_reclaimFocus = false;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    float btnW = 64.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - btnW - 8.0f);
    if (monoFont)
        ImGui::PushFont(monoFont);
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_CallbackHistory;
    if (ImGui::InputTextWithHint("##cmd", hint ? hint : "", m_inputBuf, sizeof(m_inputBuf),
                                 flags, &TextEditCallbackStub, (void *)this))
    {
        char *s = m_inputBuf;
        Strtrim(s);
        if (s[0])
        {
            AddLine(Level::Command, std::string("> ") + s);
            if (onCommand)
                onCommand(std::string(s));
            submitted = true;
        }
        s[0] = '\0';
        reclaim = true;
    }
    if (monoFont)
        ImGui::PopFont();

    ImGui::SameLine(0.0f, 8.0f);
    if (UI::Button("##send", sendLabel ? sendLabel : "发送", UI::Variant::Solid, ImVec2(btnW, 0)))
    {
        char *s = m_inputBuf;
        Strtrim(s);
        if (s[0])
        {
            AddLine(Level::Command, std::string("> ") + s);
            if (onCommand)
                onCommand(std::string(s));
            submitted = true;
        }
        s[0] = '\0';
        reclaim = true;
    }
    ImGui::PopStyleVar();

    if (reclaim)
        ImGui::SetKeyboardFocusHere(-1);
    return submitted;
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
        const int count = (int)m_history.size();
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (m_historyPos == -1)
                m_historyPos = count - 1;
            else if (m_historyPos > 0)
                m_historyPos--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (m_historyPos != -1)
                if (++m_historyPos >= count)
                    m_historyPos = -1;
        }
        if (prevPos != m_historyPos)
        {
            const char *histStr = (m_historyPos >= 0 && m_historyPos < count)
                                      ? m_history[(size_t)m_historyPos].c_str()
                                      : "";
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, histStr);
        }
        break;
    }
    }
    return 0;
}
