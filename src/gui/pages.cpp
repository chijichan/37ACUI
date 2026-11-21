// 37ACUI — 页面内容
// 概览 / 模型 / 数据集 / 节点 / 服务端 / 环境 / 设置
// 视觉规则见 theme.h 与 components.cpp（樱花拿铁设计系统）。

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "app.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
bool fileExists(const std::string &path)
{
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

const Theme::Tokens &T = Theme::T;
const Theme::Fonts &F() { return Theme::fonts(); }

std::string fmt(const char *pattern, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, pattern);
    vsnprintf(buf, sizeof(buf), pattern, ap);
    va_end(ap);
    return std::string(buf);
}

const char *accText(const std::string &value)
{
    return value.empty() ? "—" : value.c_str();
}

std::string sizeText(double mb)
{
    char buf[64];
    if (mb >= 1024.0)
        snprintf(buf, sizeof(buf), "%.2f GB", mb / 1024.0);
    else
        snprintf(buf, sizeof(buf), "%.1f MB", mb);
    return std::string(buf);
}

std::string shortSha(const std::string &sha)
{
    if (sha.empty())
        return "—";
    return sha.substr(0, 16) + "…";
}

const char *kindText(const std::string &kind)
{
    if (kind == "baseline")
        return "baseline";
    if (kind == "auto")
        return "auto";
    if (kind == "manual")
        return "manual";
    return kind.empty() ? "checkpoint" : kind.c_str();
}

ImU32 levelColor(ConsoleWidget::Level level)
{
    switch (level)
    {
    case ConsoleWidget::Level::Success: return T.successInk;
    case ConsoleWidget::Level::Warn: return T.warningInk;
    case ConsoleWidget::Level::Error: return T.dangerInk;
    case ConsoleWidget::Level::Command: return T.pink700;
    default: return T.ink700;
    }
}

// 卡片：小标题 + 大数值 + 徽章（仪表盘统计卡）
void statCard(const char *id, float width, const char *label, const std::string &value,
              const char *sub, UI::Tone tone)
{
    UI::BeginCard(id, nullptr, width);
    UI::Muted("%s", label);
    if (F().display)
        ImGui::PushFont(F().display);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink900));
    ImGui::TextUnformatted(value.c_str());
    ImGui::PopStyleColor();
    if (F().display)
        ImGui::PopFont();
    if (sub && sub[0])
        UI::Badge(sub, tone, true);
    UI::EndCard();
}

// 目录/文件行：名称 + 路径 + 状态徽章 + 打开按钮
void pathRow(const char *id, const char *label, const std::string &path, bool exists)
{
    ImGui::PushID(id);
    UI::Muted("%s", label);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(exists ? T.ink700 : T.ink300));
    if (F().mono)
        ImGui::PushFont(F().mono);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(path.c_str());
    ImGui::PopTextWrapPos();
    if (F().mono)
        ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 2.0f));
    UI::Badge(exists ? TR("common.exists") : TR("common.missing"),
              exists ? UI::Tone::Success : UI::Tone::Danger, true);
    ImGui::PopID();
}

// 设置页的可编辑路径框
bool pathInput(const char *id, char *buf, size_t size, const std::string &current,
               const char *browseTitle, std::string &out)
{
    bool changed = false;
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 92.0f);
    if (!ImGui::IsAnyItemActive())
        strncpy_s(buf, size, current.c_str(), size - 1);
    if (ImGui::InputText("##path", buf, size))
        changed = true;
    ImGui::SameLine(0.0f, 8.0f);
    if (UI::Button("##browse", TR("common.browse"), UI::Variant::Ghost, ImVec2(84, 0)))
    {
        std::string picked = PickFolder(browseTitle);
        if (!picked.empty())
        {
            strncpy_s(buf, size, picked.c_str(), size - 1);
            changed = true;
        }
    }
    ImGui::PopID();
    if (changed)
        out = std::string(buf);
    return changed;
}

} // namespace

// ===========================================================================
// 概览
// ===========================================================================
void App::renderOverviewPage()
{
    AppStateSnapshot s = snapshot();
    const float gap = T.cardGap;

    // ---- Hero ----
    UI::BeginCard("##hero", nullptr, 0.0f);
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImDrawList *dl = ImGui::GetWindowDrawList();
        dl->AddCircleFilled(ImVec2(p.x + w - 54.0f, p.y + 2.0f), 78.0f,
                            Theme::withAlpha(T.pink100, 0.55f));
        dl->AddCircleFilled(ImVec2(p.x + w - 136.0f, p.y + 66.0f), 40.0f,
                            Theme::withAlpha(T.pink50, 0.95f));

        UI::Badge(TR("overview.hero_badge"), UI::Tone::Pink, true);
        ImGui::Dummy(ImVec2(0, 6.0f));
        if (F().display)
            ImGui::PushFont(F().display);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink900));
        ImGui::TextUnformatted(TR("overview.hero_title"));
        ImGui::PopStyleColor();
        if (F().display)
            ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, 4.0f));
        ImGui::PushTextWrapPos(w * 0.72f);
        UI::Muted("%s", TR("overview.hero_desc"));
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0, 10.0f));

        // 状态胶囊
        std::string pyText = s.pythonOk ? fmt("Python %s", s.pythonVersion.c_str()) : TR("env.nofound");
        UI::StatusPill(pyText.c_str(), s.pythonOk ? UI::Tone::Success : UI::Tone::Danger);
        ImGui::SameLine(0.0f, 6.0f);
        UI::StatusPill(s.venvExists ? "venv · 就绪" : "venv · 未创建",
                       s.venvExists ? UI::Tone::Mint : UI::Tone::Neutral);
        ImGui::SameLine(0.0f, 6.0f);
        UI::StatusPill(s.projectReady ? "项目 · 就绪" : "项目 · 未安装",
                       s.projectReady ? UI::Tone::Info : UI::Tone::Neutral);
        ImGui::SameLine(0.0f, 6.0f);
        std::string modelText = (s.model.model.ok && !s.model.model.version.empty())
                                    ? ("模型 " + s.model.model.version)
                                    : "模型 · 未训练";
        UI::StatusPill(modelText.c_str(), (s.model.model.ok && !s.model.model.version.empty())
                                              ? UI::Tone::Pink
                                              : UI::Tone::Neutral);

        ImGui::Dummy(ImVec2(0, 12.0f));
        const float bw = 150.0f;
        if (UI::Button("##hero_init", TR("overview.btn_init"), UI::Variant::Solid,
                       ImVec2(bw, 40), UI::Icon::Sparkle))
        {
            if (!s.projectReady)
                startInitProject();
            else
                askConfirm(TR("env.init"), TR("overview.hint_init"), TR("env.init"), false,
                           [this]()
                           { startInitProject(); });
        }
        ImGui::SameLine(0.0f, 8.0f);
        if (UI::Button("##hero_web", TR("overview.btn_web"), UI::Variant::Ghost,
                       ImVec2(bw, 40), UI::Icon::Cloud))
            openUrl(s.frontendUrl);
        ImGui::SameLine(0.0f, 8.0f);
        if (UI::Button("##hero_console", m_consoleOpen ? TR("overview.collapse_console") : TR("overview.open_console"),
                       UI::Variant::Text, ImVec2(0, 40)))
            m_consoleOpen = !m_consoleOpen;
    }
    UI::EndCard();
    ImGui::Dummy(ImVec2(0, gap));

    // ---- 统计卡 ----
    {
        float avail = ImGui::GetContentRegionAvail().x;
        float cw = (avail - gap * 3.0f) / 4.0f;
        if (cw < 150.0f)
            cw = AcMax(150.0f, (avail - gap) / 2.0f);
        ImVec2 start = ImGui::GetCursorScreenPos();
        float maxY = start.y;

        struct StatDef
        {
            const char *id;
            const char *label;
            std::string value;
            std::string sub;
            UI::Tone tone;
        };
        std::vector<StatDef> stats = {
            {"##st_py", TR("overview.stat_python"),
             s.pythonOk ? s.pythonVersion : std::string(TR("overview.stat_none")),
             s.pythonOk ? "已检测" : "需要安装",
             s.pythonOk ? UI::Tone::Success : UI::Tone::Danger},
            {"##st_model", TR("overview.stat_model"),
             (s.model.model.ok && !s.model.model.version.empty()) ? s.model.model.version
                                                                  : std::string(TR("overview.stat_none")),
             (s.model.model.ok && !s.model.model.base.empty()) ? s.model.model.base : "saves/models",
             UI::Tone::Pink},
            {"##st_class", TR("overview.stat_classes"),
             s.model.model.ok && s.model.model.classCount > 0 ? std::to_string(s.model.model.classCount)
                                                              : std::string("—"),
             "训练分类", UI::Tone::Info},
            {"##st_ckpt", TR("overview.stat_checkpoints"),
             s.model.model.ok ? std::to_string((int)s.model.checkpoints.size()) : std::string("—"),
             "可回滚快照", UI::Tone::Mint},
        };

        for (size_t i = 0; i < stats.size(); i++)
        {
            if (i > 0)
                ImGui::SameLine(0.0f, gap);
            ImGui::BeginGroup();
            statCard(stats[i].id, cw, stats[i].label, stats[i].value,
                     stats[i].sub.c_str(), stats[i].tone);
            ImGui::EndGroup();
            maxY = AcMax(maxY, ImGui::GetItemRectMax().y);
        }
        ImGui::SetCursorScreenPos(ImVec2(start.x, maxY + gap));
    }

    // ---- 快速开始 / 运行状态 ----
    renderTwoColumn(
        [&](float colW)
        {
            UI::BeginCard("##quick", TR("overview.quick"), colW, TR("overview.quick_desc"),
                          UI::Icon::Sparkle);
            {
                float bw = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
                if (UI::Button("##q_train", TR("overview.btn_train"), UI::Variant::Solid,
                               ImVec2(bw, 38), UI::Icon::Play))
                    startTrain(false);
                ImGui::SameLine(0.0f, 8.0f);
                if (UI::Button("##q_dataset", TR("overview.btn_dataset"), UI::Variant::Ghost,
                               ImVec2(bw, 38), UI::Icon::Database))
                    goPage(Page::Dataset);

                if (UI::Button("##q_server", TR("overview.btn_server"), UI::Variant::Ghost,
                               ImVec2(bw, 38), UI::Icon::Server))
                    goPage(Page::Server);
                ImGui::SameLine(0.0f, 8.0f);
                if (UI::Button("##q_node", TR("overview.btn_node"), UI::Variant::Ghost,
                               ImVec2(bw, 38), UI::Icon::Network))
                    goPage(Page::Node);

                if (UI::Button("##q_open", TR("overview.btn_download"), UI::Variant::Soft,
                               ImVec2(bw, 38), UI::Icon::Download))
                {
                    if (s.projectReady)
                        askConfirm(TR("git.clone"), TR("overview.hint_init"), TR("git.clone"), true,
                                   [this]()
                                   { startGitDownload(); });
                    else
                        startGitDownload();
                }
                ImGui::SameLine(0.0f, 8.0f);
                if (UI::Button("##q_menu", TR("console.title"), UI::Variant::Soft,
                               ImVec2(bw, 38), UI::Icon::Terminal))
                    m_consoleOpen = !m_consoleOpen;

                ImGui::Dummy(ImVec2(0, 4.0f));
                UI::Muted("%s", TR("overview.hint_init"));
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            UI::BeginCard("##recent", TR("overview.recent"), colW, TR("overview.recent_desc"),
                          UI::Icon::Terminal, TR("overview.open_console"), nullptr);
            {
                const int rows = 6;
                auto tail = m_console.Tail((size_t)rows);
                if (tail.empty())
                {
                    UI::Muted("%s", TR("console.empty"));
                }
                else
                {
                    for (auto &line : tail)
                    {
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        ImGui::GetWindowDrawList()->AddCircleFilled(
                            ImVec2(p.x + 4.0f, p.y + 8.0f), 2.6f,
                            Theme::withAlpha(levelColor(line.level), 0.9f));
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 14.0f);
                        if (F().mono)
                            ImGui::PushFont(F().mono);
                        ImGui::PushStyleColor(ImGuiCol_Text,
                                              ImGui::ColorConvertU32ToFloat4(levelColor(line.level)));
                        std::string text = line.time + "  " + line.text;
                        ImGui::PushTextWrapPos(0.0f);
                        ImGui::TextUnformatted(text.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::PopStyleColor();
                        if (F().mono)
                            ImGui::PopFont();
                    }
                }
            }
            UI::EndCard();
        },
        [&](float colW)
        {
            UI::BeginCard("##runtime", TR("overview.runtime"), colW, TR("overview.runtime_desc"),
                          UI::Icon::Server);
            {
                UI::KV(TR("overview.proc"),
                       s.procRunning ? (s.procName.empty() ? TR("common.running") : s.procName.c_str())
                                     : TR("overview.proc_none"),
                       true, 88.0f);
                ImGui::Dummy(ImVec2(0, 4.0f));
                {
                    ImGui::PushFont(F().caption);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink500));
                    ImGui::TextUnformatted(TR("overview.ports"));
                    ImGui::PopStyleColor();
                    ImGui::PopFont();
                    ImGui::SameLine(88.0f);
                    UI::StatusPill(fmt("Flask :%d", webPort()).c_str(),
                                   s.webOnline ? UI::Tone::Success : UI::Tone::Neutral);
                    ImGui::SameLine(0.0f, 6.0f);
                    UI::StatusPill(fmt("TCP :%d", tcpPort()).c_str(),
                                   s.tcpOnline ? UI::Tone::Success : UI::Tone::Neutral);
                }
                ImGui::Dummy(ImVec2(0, 8.0f));
                UI::Muted("%s", TR("status.project"));
                if (F().mono)
                    ImGui::PushFont(F().mono);
                ImGui::PushTextWrapPos(0.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink700));
                ImGui::TextUnformatted(s.projectRoot.c_str());
                ImGui::PopStyleColor();
                ImGui::PopTextWrapPos();
                if (F().mono)
                    ImGui::PopFont();

                ImGui::Dummy(ImVec2(0, 8.0f));
                if (UI::Button("##rt_open", TR("overview.btn_web"), UI::Variant::Ghost,
                               ImVec2(-1, 36), UI::Icon::Cloud))
                    openUrl(s.frontendUrl);
            }
            UI::EndCard();
        });
}

// ===========================================================================
// 模型
// ===========================================================================
void App::renderModelPage()
{
    AppStateSnapshot s = snapshot();
    const float gap = T.cardGap;
    const Bridge::ModelInfo &m = s.model.model;

    renderTwoColumn(
        [&](float colW)
        {
            bool action = false;
            UI::BeginCard("##model_current", TR("model.current"), colW, TR("model.current_desc"),
                          UI::Icon::Cube, TR("common.open_dir"), &action);
            if (action)
                openPath(modelDir());
            {
                if (!m.ok && !s.modelLoading)
                {
                    UI::EmptyState(UI::Icon::Cube, TR("model.no_model"),
                                   m.error.empty() ? TR("model.no_model_desc") : m.error.c_str());
                    if (UI::Button("##retry", TR("common.refresh"), UI::Variant::Soft,
                                   ImVec2(-1, 34), UI::Icon::Refresh))
                        refreshModelInfo();
                }
                else if (s.modelLoading && !m.ok)
                {
                    UI::EmptyState(UI::Icon::Refresh, TR("common.loading"), nullptr);
                }
                else
                {
                    // 顶部徽章
                    UI::Badge(m.version.empty() ? "unknown" : m.version.c_str(), UI::Tone::Pink);
                    ImGui::SameLine(0.0f, 6.0f);
                    UI::Badge(fmt("%d 类", m.classCount).c_str(), UI::Tone::Info);
                    ImGui::SameLine(0.0f, 6.0f);
                    UI::Badge(m.base.empty() ? "base?" : m.base.c_str(), UI::Tone::Mint);
                    if (m.checkpointCount > 0)
                    {
                        ImGui::SameLine(0.0f, 6.0f);
                        UI::Badge(fmt("%d 检查点", m.checkpointCount).c_str(), UI::Tone::Neutral);
                    }
                    ImGui::Dummy(ImVec2(0, 8.0f));

                    UI::KV(TR("model.version"), m.version.empty() ? "—" : m.version.c_str(), true);
                    UI::KV(TR("model.trained_at"), m.trainedAt.empty() ? "—" : m.trainedAt.c_str(), true);
                    UI::KV(TR("model.image_size"), m.imageSize.empty() ? "—" : m.imageSize.c_str(), true);
                    UI::KV(TR("model.best_acc"), m.bestAcc.empty() ? "—" : fmt("%s%%", m.bestAcc.c_str()).c_str(), true);
                    UI::KV(TR("model.size"), sizeText(m.sizeMb).c_str(), true);
                    UI::KV(TR("model.dataset"), m.dataset.empty() ? "—" : m.dataset.c_str(), true);
                    UI::KV(TR("model.sha"), shortSha(m.sha).c_str(), true);

                    ImGui::Dummy(ImVec2(0, 6.0f));
                    if (!m.modelExists)
                        UI::Badge(TR("common.missing"), UI::Tone::Danger, true);
                }
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            // ---- 检查点 ----
            bool prune = false;
            UI::BeginCard("##model_ckpt", TR("model.ckpt"), colW, TR("model.ckpt_desc"),
                          UI::Icon::Rotate, TR("model.ckpt_prune"), &prune);
            if (prune)
            {
                askConfirm(TR("model.ckpt_prune_title"), TR("model.ckpt_prune_body"),
                           TR("model.ckpt_prune"), false,
                           [this]()
                           { runBridge(TR("action.ckpt_prune"), "ckpt-prune 10 2000"); });
            }
            {
                if (s.model.checkpoints.empty())
                {
                    UI::EmptyState(UI::Icon::Rotate, TR("model.ckpt_empty"), TR("model.ckpt_empty_desc"));
                }
                else
                {
                    ImGui::BeginChild("##ckptlist", ImVec2(0, 268), false,
                                      ImGuiWindowFlags_None);
                    for (size_t i = 0; i < s.model.checkpoints.size(); i++)
                    {
                        const Bridge::Checkpoint &c = s.model.checkpoints[i];
                        ImGui::PushID((int)i);
                        float availW = ImGui::GetContentRegionAvail().x;
                        ImVec2 rowStart = ImGui::GetCursorScreenPos();

                        // 名称
                        std::string shown = c.name;
                        ImGui::PushFont(F().bodyBold);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink900));
                        ImGui::PushTextWrapPos(availW - 132.0f);
                        ImGui::TextUnformatted(shown.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::PopStyleColor();
                        ImGui::PopFont();

                        // 右侧操作
                        float btnW = 56.0f;
                        float btnH = 26.0f;
                        ImGui::SetCursorScreenPos(ImVec2(rowStart.x + availW - btnW * 2.0f - 6.0f,
                                                         rowStart.y - 2.0f));
                        if (UI::Button("##restore", TR("model.ckpt_restore"), UI::Variant::Ghost,
                                       ImVec2(btnW, btnH), UI::Icon::Rotate, !c.exists))
                        {
                            std::string name = c.name;
                            askConfirm(TR("model.ckpt_restore_title"), TR("model.ckpt_restore_body"),
                                       TR("model.ckpt_restore"), false,
                                       [this, name]()
                                       { runBridge(TR("action.ckpt_restore"), "ckpt-restore \"" + name + "\""); });
                        }
                        ImGui::SameLine(0.0f, 6.0f);
                        if (UI::Button("##delete", TR("model.ckpt_delete"), UI::Variant::Text,
                                       ImVec2(btnW, btnH), UI::Icon::Trash))
                        {
                            std::string name = c.name;
                            askConfirm(TR("model.ckpt_delete_title"), TR("model.ckpt_delete_body"),
                                       TR("model.ckpt_delete"), true,
                                       [this, name]()
                                       { runBridge(TR("action.ckpt_delete"), "ckpt-delete \"" + name + "\""); });
                        }

                        // 元信息行
                        ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + 24.0f));
                        UI::Tone kindTone = UI::Tone::Neutral;
                        if (c.kind == "best" || c.kind == "final")
                            kindTone = UI::Tone::Success;
                        else if (c.kind == "interrupt")
                            kindTone = UI::Tone::Warning;
                        UI::Badge(kindText(c.kind), kindTone, true);
                        ImGui::SameLine(0.0f, 6.0f);
                        std::string acc = "—";
                        if (!c.accuracy.empty())
                        {
                            char accBuf[32];
                            snprintf(accBuf, sizeof(accBuf), "acc %.2f%%", atof(c.accuracy.c_str()));
                            acc = accBuf;
                        }
                        UI::Muted("%s · %s", sizeText(c.sizeMb).c_str(), acc.c_str());
                        if (F().caption)
                            ImGui::PushFont(F().caption);
                        ImGui::SameLine(0.0f, 8.0f);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink300));
                        ImGui::TextUnformatted(c.createdAt.c_str());
                        ImGui::PopStyleColor();
                        if (F().caption)
                            ImGui::PopFont();
                        if (!c.exists)
                        {
                            ImGui::SameLine(0.0f, 8.0f);
                            UI::Badge(TR("model.ckpt_missing"), UI::Tone::Danger, true);
                        }

                        ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + 48.0f));
                        UI::Divider();
                        ImGui::Dummy(ImVec2(0, 6.0f));
                        ImGui::PopID();
                    }
                    ImGui::EndChild();
                }
            }
            UI::EndCard();
        },
        [&](float colW)
        {
            // ---- 训练 ----
            UI::BeginCard("##model_train", TR("model.train"), colW, TR("model.train_desc"),
                          UI::Icon::Play);
            {
                UI::Muted("%s", TR("model.dataset_choice"));
                ImGui::Dummy(ImVec2(0, 2.0f));
                const char *items[] = {TR("model.dataset_raw"), TR("model.dataset_cropped"),
                                       TR("model.dataset_yolo")};
                int choice = UI::Segmented("##dataset_choice", items, 3, m_datasetChoice, 0.0f);
                if (choice != m_datasetChoice)
                {
                    m_datasetChoice = choice;
                    markDirty();
                }
                std::string ds = (m_datasetChoice == 1) ? croppedDir() : datasetDir();
                ImGui::Dummy(ImVec2(0, 4.0f));
                if (F().mono)
                    ImGui::PushFont(F().mono);
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink300));
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextUnformatted(ds.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
                if (F().mono)
                    ImGui::PopFont();
                ImGui::Dummy(ImVec2(0, 8.0f));

                float bw = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
                if (UI::Button("##train", TR("model.btn_train"), UI::Variant::Solid,
                               ImVec2(bw, 40), UI::Icon::Play))
                    startTrain(false);
                ImGui::SameLine(0.0f, 8.0f);
                if (UI::Button("##resume", TR("model.btn_resume"), UI::Variant::Soft,
                               ImVec2(bw, 40), UI::Icon::Refresh))
                    startTrain(true);
                ImGui::Dummy(ImVec2(0, 4.0f));
                UI::Muted("%s", TR("model.train_hint"));
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            // ---- 训练基模 ----
            UI::BeginCard("##model_base", TR("model.base_title"), colW, TR("model.base_desc"),
                          UI::Icon::Cube);
            {
                std::string current = EnvOr(s.envCli, "PRETRAINED_BASE", "imagenet-resnet18");
                UI::Muted("%s", TR("model.base_current"));
                ImGui::SameLine(0.0f, 8.0f);
                UI::Badge(current.c_str(), UI::Tone::Pink, true);
                ImGui::Dummy(ImVec2(0, 6.0f));
                struct BaseDef
                {
                    const char *id;
                    const char *label;
                    const char *detail;
                };
                const BaseDef bases[] = {
                    {"imagenet-resnet18", "ImageNet · ResNet18", "内置 · 零下载 · 224"},
                    {"imagenet-efficientnet_b0", "ImageNet · EfficientNet-B0", "内置 · 零下载 · 224"},
                    {"imagenet-efficientnet_b3", "ImageNet · EfficientNet-B3", "内置 · 零下载 · 300"},
                };
                for (const auto &b : bases)
                {
                    ImGui::PushID(b.id);
                    bool isCurrent = (current == b.id);
                    ImVec2 rowPos = ImGui::GetCursorScreenPos();
                    float rowW = ImGui::GetContentRegionAvail().x;
                    ImGui::PushFont(F().bodyBold);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(
                                                              isCurrent ? T.pink700 : T.ink900));
                    ImGui::TextUnformatted(b.label);
                    ImGui::PopStyleColor();
                    ImGui::PopFont();
                    UI::Muted("%s", b.detail);
                    if (isCurrent)
                    {
                        ImGui::SameLine(0.0f, 8.0f);
                        UI::Badge(TR("model.base_current"), UI::Tone::Pink, true);
                    }
                    else
                    {
                        float bw = 66.0f;
                        ImGui::SetCursorScreenPos(ImVec2(rowPos.x + rowW - bw, rowPos.y));
                        if (UI::Button("##use", TR("common.start"), UI::Variant::Text,
                                       ImVec2(bw, 26)))
                        {
                            if (writeEnvValue(cliRoot(), "PRETRAINED_BASE", b.id))
                            {
                                refreshEnv();
                                addSuccess(std::string("[env] PRETRAINED_BASE=") + b.id);
                                notify(UI::Tone::Success, TR("toast.reload_base"));
                            }
                            else
                            {
                                notify(UI::Tone::Danger, TR("env.env_fail"));
                            }
                        }
                    }
                    ImGui::Dummy(ImVec2(0, 2.0f));
                    UI::Divider();
                    ImGui::Dummy(ImVec2(0, 6.0f));
                    ImGui::PopID();
                }

                float bw = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
                if (UI::Button("##dl", TR("model.base_download"), UI::Variant::Ghost,
                               ImVec2(bw, 34), UI::Icon::Download))
                {
                    askInput(TR("model.base_download_title"), TR("model.base_download_label"), "",
                             false, TR("model.base_download"),
                             [this](const std::string &repo)
                             {
                                 if (repo.find('/') == std::string::npos)
                                 {
                                     notify(UI::Tone::Warning, TR("toast.need_repo"));
                                     return;
                                 }
                                 runBridge(TR("action.base_download"), "base-download \"" + repo + "\"");
                             });
                }
                ImGui::SameLine(0.0f, 8.0f);
                if (UI::Button("##login", TR("model.base_login"), UI::Variant::Ghost,
                               ImVec2(bw, 34), UI::Icon::Cloud))
                {
                    askInput(TR("model.hf_login_title"), TR("model.hf_login_label"), "", true,
                             TR("model.base_login"),
                             [this](const std::string &token)
                             {
                                 if (token.empty())
                                 {
                                     notify(UI::Tone::Warning, TR("toast.need_token"));
                                     return;
                                 }
                                 m_hfToken = token;
                                 _putenv_s("HF_TOKEN", token.c_str());
                                 runBridge(TR("action.hf_login"), "hf-login");
                             });
                }
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            // ---- 发布 ----
            UI::BeginCard("##model_publish", TR("model.publish"), colW, TR("model.publish_desc"),
                          UI::Icon::Upload);
            {
                UI::Muted("%s", TR("model.publish_repo"));
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##repo", "yourname/37ac", m_repoBuf, sizeof(m_repoBuf));
                ImGui::Dummy(ImVec2(0, 6.0f));
                if (UI::Button("##publish", TR("model.btn_publish"), UI::Variant::Solid,
                               ImVec2(-1, 38), UI::Icon::Upload))
                {
                    std::string repo(m_repoBuf);
                    if (repo.find('/') == std::string::npos)
                    {
                        notify(UI::Tone::Warning, TR("toast.need_repo"));
                    }
                    else
                    {
                        askInput(TR("model.publish"), TR("model.publish_token"),
                                 m_hfToken, true, TR("model.btn_publish"),
                                 [this, repo](const std::string &token)
                                 {
                                     if (!token.empty())
                                     {
                                         m_hfToken = token;
                                         _putenv_s("HF_TOKEN", token.c_str());
                                     }
                                     runBridge(TR("action.publish"), "publish \"" + repo + "\"");
                                 });
                    }
                }
                UI::Muted("%s", TR("model.publish_hint"));
            }
            UI::EndCard();
        });
}

// ===========================================================================
// 数据集
// ===========================================================================
void App::renderDatasetPage()
{
    AppStateSnapshot s = snapshot();
    const float gap = T.cardGap;
    std::string raw = datasetDir();
    std::string cropped = croppedDir();
    bool rawExists = fileExists(raw);
    bool croppedExists = fileExists(cropped);

    renderTwoColumn(
        [&](float colW)
        {
            UI::BeginCard("##ds_paths", TR("dataset.paths"), colW, TR("dataset.paths_desc"),
                          UI::Icon::Folder);
            {
                pathRow("raw", TR("dataset.raw"), raw, rawExists);
                ImGui::Dummy(ImVec2(0, 8.0f));
                pathRow("cropped", TR("dataset.cropped"), cropped, croppedExists);
                ImGui::Dummy(ImVec2(0, 10.0f));
                float bw = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
                if (UI::Button("##open_raw", TR("common.open_dir"), UI::Variant::Ghost,
                               ImVec2(bw, 34), UI::Icon::Folder))
                    openPath(raw);
                ImGui::SameLine(0.0f, 8.0f);
                if (UI::Button("##open_cropped", TR("common.open_dir"), UI::Variant::Ghost,
                               ImVec2(bw, 34), UI::Icon::Folder))
                    openPath(cropped);
                ImGui::Dummy(ImVec2(0, 4.0f));
                if (!rawExists)
                    UI::Muted("%s", TR("dataset.missing_raw"));
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            UI::BeginCard("##ds_pipeline", TR("dataset.pipeline"), colW, nullptr,
                          UI::Icon::Sparkle);
            {
                UI::WrapHint(TR("dataset.pipeline_desc"));
                ImGui::Dummy(ImVec2(0, 8.0f));
                const std::pair<const char *, UI::Tone> steps[] = {
                    {"1 · 原始图片", UI::Tone::Neutral},
                    {"2 · YOLO 裁剪", UI::Tone::Pink},
                    {"3 · 压缩 / JPEG", UI::Tone::Info},
                    {"4 · 训练", UI::Tone::Success},
                };
                for (auto &step : steps)
                {
                    UI::StatusPill(step.first, step.second, true);
                    ImGui::SameLine(0.0f, 6.0f);
                    if (F().caption)
                        ImGui::PushFont(F().caption);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink300));
                    ImGui::TextUnformatted("→");
                    ImGui::PopStyleColor();
                    if (F().caption)
                        ImGui::PopFont();
                    ImGui::SameLine(0.0f, 6.0f);
                }
                ImGui::NewLine();
            }
            UI::EndCard();
        },
        [&](float colW)
        {
            UI::BeginCard("##ds_ops", TR("dataset.ops"), colW, TR("dataset.ops_desc"),
                          UI::Icon::Sliders);
            {
                auto opRow = [&](const char *id, UI::Icon icon, const char *title, const char *desc,
                                 const char *btnLabel, UI::Variant variant,
                                 std::function<void()> onClick)
                {
                    ImGui::PushID(id);
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    float rowW = ImGui::GetContentRegionAvail().x;
                    UI::IconBox(icon, 34.0f, UI::Tone::Pink, 0.5f);
                    ImGui::SameLine(0.0f, 10.0f);
                    ImGui::BeginGroup();
                    if (F().bodyBold)
                        ImGui::PushFont(F().bodyBold);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink900));
                    ImGui::TextUnformatted(title);
                    ImGui::PopStyleColor();
                    if (F().bodyBold)
                        ImGui::PopFont();
                    UI::Muted("%s", desc);
                    ImGui::EndGroup();

                    ImGui::SameLine(0.0f, 8.0f);
                    float bw = 74.0f;
                    ImGui::SetCursorScreenPos(ImVec2(p.x + rowW - bw, p.y + 2.0f));
                    if (UI::Button("##run", btnLabel, variant, ImVec2(bw, 30)))
                        onClick();
                    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 34.0f + 8.0f));
                    UI::Divider();
                    ImGui::Dummy(ImVec2(0, 8.0f));
                    ImGui::PopID();
                };

                opRow("verify", UI::Icon::Check, TR("dataset.verify"), TR("dataset.verify_desc"),
                      TR("common.start"), UI::Variant::Ghost,
                      [this]()
                      { runBridge(TR("action.verify"), "dataset-verify"); });

                opRow("crop", UI::Icon::Sparkle, TR("dataset.crop"), TR("dataset.crop_desc"),
                      TR("common.start"), UI::Variant::Soft,
                      [this]()
                      {
                          askConfirm(TR("dataset.crop_title"), TR("dataset.crop_body"),
                                     TR("common.start"), false,
                                     [this]()
                                     { runBridge(TR("action.crop"), "dataset-crop"); });
                      });

                opRow("compress", UI::Icon::Download, TR("dataset.compress"),
                      TR("dataset.compress_desc"), TR("common.start"), UI::Variant::Ghost,
                      [this]()
                      {
                          askConfirm(TR("dataset.compress_title"), TR("dataset.compress_body"),
                                     TR("common.start"), false,
                                     [this]()
                                     { runBridge(TR("action.compress"), "dataset-compress"); });
                      });

                opRow("jpeg", UI::Icon::Sparkle, TR("dataset.jpeg"), TR("dataset.jpeg_desc"),
                      TR("common.start"), UI::Variant::Ghost,
                      [this]()
                      {
                          askConfirm(TR("dataset.jpeg_title"), TR("dataset.jpeg_body"),
                                     TR("common.start"), false,
                                     [this]()
                                     { runBridge(TR("action.jpeg"), "dataset-jpeg"); });
                      });
            }
            UI::EndCard();
        });
}

// ===========================================================================
// 节点
// ===========================================================================
void App::renderNodePage()
{
    AppStateSnapshot s = snapshot();
    const float gap = T.cardGap;
    std::string token = EnvOr(s.envCli, "TOKEN", "");
    std::string serverUrl = EnvOr(s.envCli, "SERVER_HTTP_URL", "http://127.0.0.1:13138");
    std::string tcpHost = EnvOr(s.envCli, "TCP_HOST", "127.0.0.1");
    std::string tcpPortStr = EnvOr(s.envCli, "TCP_PORT", "13137");
    std::string nodeId = EnvOr(s.envCli, "NODE_ID", "1");
    std::string maxTasks = EnvOr(s.envCli, "MAX_TASKS", "5");
    std::string heartbeat = EnvOr(s.envCli, "HEARTBEAT_INTERVAL_SEC", "15");
    std::string autoUpdate = EnvOr(s.envCli, "AUTO_UPDATE_MODEL", "True");

    renderTwoColumn(
        [&](float colW)
        {
            bool isNode = s.procRunning && s.procName.rfind("node", 0) == 0;
            UI::BeginCard("##node_service", TR("node.service"), colW, TR("node.service_desc"),
                          UI::Icon::Network);
            {
                UI::StatusPill(isNode ? TR("common.running") : TR("common.idle"),
                               isNode ? UI::Tone::Success : UI::Tone::Neutral);
                ImGui::SameLine(0.0f, 8.0f);
                if (!token.empty())
                    UI::Badge(TR("node.token_set"), UI::Tone::Success, true);
                else
                    UI::Badge(TR("node.token_missing"), UI::Tone::Warning, true);
                ImGui::Dummy(ImVec2(0, 10.0f));

                if (isNode)
                {
                    if (UI::Button("##node_stop", TR("node.stop"), UI::Variant::Danger,
                                   ImVec2(-1, 40), UI::Icon::Stop))
                        stopProcess();
                }
                else
                {
                    if (UI::Button("##node_start", TR("node.start"), UI::Variant::Solid,
                                   ImVec2(-1, 40), UI::Icon::Play))
                        startNodeService();
                }
                ImGui::Dummy(ImVec2(0, 6.0f));
                UI::Toggle("##no_update", &m_nodeNoUpdate, TR("node.no_update"));
                ImGui::Dummy(ImVec2(0, 4.0f));
                if (token.empty())
                    UI::Muted("%s", TR("node.not_configured_hint"));
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            UI::BeginCard("##node_config", TR("node.config"), colW, TR("node.config_desc"),
                          UI::Icon::Sliders);
            {
                UI::KV(TR("node.id"), nodeId.c_str(), true);
                UI::KV(TR("node.tcp"), (tcpHost + ":" + tcpPortStr).c_str(), true);
                UI::KV(TR("node.server"), serverUrl.c_str(), true);
                UI::KV(TR("node.token"), token.empty() ? TR("node.token_missing") : TR("node.token_set"), false);
                UI::KV(TR("node.max_tasks"), maxTasks.c_str(), true);
                UI::KV(TR("node.heartbeat"), (heartbeat + " s").c_str(), true);
                UI::KV(TR("env.auto_update"), autoUpdate.c_str(), false);
                ImGui::Dummy(ImVec2(0, 10.0f));
                if (UI::Button("##open_env", TR("env.open_env"), UI::Variant::Ghost,
                               ImVec2(-1, 34), UI::Icon::Folder))
                    openTextFile(cliRoot() + "/.env");
            }
            UI::EndCard();
        },
        [&](float colW)
        {
            UI::BeginCard("##node_conn", TR("node.connectivity"), colW, TR("node.connectivity_desc"),
                          UI::Icon::Server);
            {
                auto connRow = [&](const char *label, int port, bool online)
                {
                    ImGui::PushID(label);
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    if (F().bodyBold)
                        ImGui::PushFont(F().bodyBold);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink900));
                    ImGui::TextUnformatted(label);
                    ImGui::PopStyleColor();
                    if (F().bodyBold)
                        ImGui::PopFont();
                    UI::Muted("端口 %d", port);
                    ImGui::SameLine(0.0f, 8.0f);
                    UI::StatusPill(online ? TR("common.online") : TR("common.offline"),
                                   online ? UI::Tone::Success : UI::Tone::Neutral);
                    ImGui::Dummy(ImVec2(0, 6.0f));
                    ImGui::PopID();
                    (void)p;
                };
                connRow(TR("node.web_port"), webPort(), s.webOnline);
                connRow(TR("node.tcp_port"), tcpPort(), s.tcpOnline);
                ImGui::Dummy(ImVec2(0, 6.0f));
                if (UI::Button("##upload", TR("server.link_upload"), UI::Variant::Ghost,
                               ImVec2(-1, 36), UI::Icon::Cloud))
                    openUrl(backendUrl() + "/upload");
                ImGui::Dummy(ImVec2(0, 8.0f));
                UI::WrapHint(TR("node.hint"));
            }
            UI::EndCard();
        });
}

// ===========================================================================
// 服务端
// ===========================================================================
void App::renderServerPage()
{
    AppStateSnapshot s = snapshot();
    const float gap = T.cardGap;
    std::string dbHost = EnvOr(s.envServer, "DB_HOST", "未配置");
    std::string webHostStr = EnvOr(s.envServer, "WEB_HOST", "127.0.0.1");

    renderTwoColumn(
        [&](float colW)
        {
            bool isServer = s.procRunning && s.procName == "server";
            UI::BeginCard("##srv_control", TR("server.control"), colW, TR("server.control_desc"),
                          UI::Icon::Server);
            {
                UI::StatusPill(isServer ? TR("common.running") : TR("common.idle"),
                               isServer ? UI::Tone::Success : UI::Tone::Neutral);
                ImGui::SameLine(0.0f, 8.0f);
                UI::StatusPill(s.webOnline ? TR("common.online") : TR("common.offline"),
                               s.webOnline ? UI::Tone::Success : UI::Tone::Neutral);
                ImGui::Dummy(ImVec2(0, 10.0f));
                if (isServer)
                {
                    if (UI::Button("##srv_stop", TR("server.stop"), UI::Variant::Danger,
                                   ImVec2(-1, 40), UI::Icon::Stop))
                        stopProcess();
                }
                else
                {
                    if (UI::Button("##srv_start", TR("server.start"), UI::Variant::Solid,
                                   ImVec2(-1, 40), UI::Icon::Play))
                        startServer();
                }
                ImGui::Dummy(ImVec2(0, 8.0f));
                UI::KV(TR("server.web_port"), fmt("%s:%d", webHostStr.c_str(), webPort()).c_str(), true);
                UI::KV(TR("server.tcp_port"), fmt("%d", tcpPort()).c_str(), true);
                UI::KV(TR("server.db_host"), dbHost.c_str(), true);
                ImGui::Dummy(ImVec2(0, 8.0f));
                if (!s.webOnline)
                    UI::Muted("%s", TR("server.offline_hint"));
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            UI::BeginCard("##srv_ports", TR("server.ports"), colW, nullptr, UI::Icon::Network);
            {
                auto row = [&](const char *label, int port, bool online)
                {
                    ImGui::PushID(label);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink700));
                    ImGui::TextUnformatted(label);
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0.0f, 8.0f);
                    UI::Muted(":%d", port);
                    ImGui::SameLine(0.0f, 8.0f);
                    UI::StatusPill(online ? TR("common.online") : TR("common.offline"),
                                   online ? UI::Tone::Success : UI::Tone::Neutral);
                    if (F().mono)
                        ImGui::PushFont(F().mono);
                    ImGui::SameLine(0.0f, 8.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink300));
                    ImGui::TextUnformatted(backendUrl().c_str());
                    ImGui::PopStyleColor();
                    if (F().mono)
                        ImGui::PopFont();
                    ImGui::Dummy(ImVec2(0, 2.0f));
                    ImGui::PopID();
                };
                row("Flask", webPort(), s.webOnline);
                row("TCP", tcpPort(), s.tcpOnline);
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            UI::BeginCard("##srv_hint", TR("common.hint"), colW, nullptr, UI::Icon::Info);
            UI::WrapHint(TR("server.hint"));
            UI::EndCard();
        },
        [&](float colW)
        {
            UI::BeginCard("##srv_links", TR("server.links"), colW, TR("server.links_desc"),
                          UI::Icon::Cloud);
            {
                std::string fe = s.frontendUrl;
                std::string api = backendUrl();
                struct LinkDef
                {
                    const char *id;
                    const char *label;
                    std::string url;
                    UI::Icon icon;
                    UI::Tone tone;
                };
                std::vector<LinkDef> links = {
                    {"##l_site", TR("server.link_site"), fe, UI::Icon::Grid, UI::Tone::Pink},
                    {"##l_upload", TR("server.link_upload"), fe + "/upload", UI::Icon::Upload, UI::Tone::Mint},
                    {"##l_dash", TR("server.link_dashboard"), fe + "/dashboard", UI::Icon::Sliders, UI::Tone::Info},
                    {"##l_api", TR("server.link_api"), api + "/upload", UI::Icon::Server, UI::Tone::Neutral},
                };
                for (auto &l : links)
                {
                    ImGui::PushID(l.id);
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    float rowW = ImGui::GetContentRegionAvail().x;
                    UI::IconBox(l.icon, 34.0f, l.tone, 0.5f);
                    ImGui::SameLine(0.0f, 10.0f);
                    ImGui::BeginGroup();
                    if (F().bodyBold)
                        ImGui::PushFont(F().bodyBold);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink900));
                    ImGui::TextUnformatted(l.label);
                    ImGui::PopStyleColor();
                    if (F().bodyBold)
                        ImGui::PopFont();
                    if (F().mono)
                        ImGui::PushFont(F().mono);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink500));
                    ImGui::TextUnformatted(l.url.c_str());
                    ImGui::PopStyleColor();
                    if (F().mono)
                        ImGui::PopFont();
                    ImGui::EndGroup();

                    float bw = 74.0f;
                    ImGui::SameLine(0.0f, 8.0f);
                    ImGui::SetCursorScreenPos(ImVec2(p.x + rowW - bw, p.y + 4.0f));
                    if (UI::Button("##go", TR("common.open"), UI::Variant::Text, ImVec2(bw, 28)))
                        openUrl(l.url);
                    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 42.0f));
                    ImGui::PopID();
                }
                ImGui::Dummy(ImVec2(0, 4.0f));
                UI::WrapHint(TR("server.hint"));
            }
            UI::EndCard();
        });
}

// ===========================================================================
// 环境
// ===========================================================================
void App::renderEnvPage()
{
    AppStateSnapshot s = snapshot();
    const float gap = T.cardGap;
    std::string proot = s.projectRoot;
    std::string cli = s.cliRoot;
    std::string srv = s.serverRoot;

    renderTwoColumn(
        [&](float colW)
        {
            bool recheck = false;
            UI::BeginCard("##env_python", TR("env.python"), colW, TR("env.python_desc"),
                          UI::Icon::Terminal, TR("common.recheck"), &recheck);
            if (recheck)
            {
                checkPython();
                notify(UI::Tone::Info, TR("env.python"));
            }
            {
                UI::StatusDot(s.pythonOk ? UI::Tone::Success : UI::Tone::Danger, 4.0f, true);
                ImGui::SameLine(0.0f, 4.0f);
                UI::Strong("%s", s.pythonOk ? TR("env.ready") : TR("env.nofound"));
                ImGui::SameLine(0.0f, 8.0f);
                if (s.pythonOk)
                    UI::Badge(fmt("v%s", s.pythonVersion.c_str()).c_str(), UI::Tone::Info, true);
                ImGui::Dummy(ImVec2(0, 6.0f));
                UI::KV("venv", s.venvExists ? TR("common.ready") : TR("common.missing"), false);
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            UI::BeginCard("##env_paths", TR("env.paths"), colW, TR("env.paths_desc"), UI::Icon::Folder);
            {
                UI::KV(TR("env.root"), proot.c_str(), true);
                UI::KV(TR("env.cli"), cli.c_str(), true);
                UI::KV(TR("env.srv"), srv.c_str(), true);
                ImGui::Dummy(ImVec2(0, 8.0f));
                if (UI::Button("##open_project", TR("common.open_dir"), UI::Variant::Ghost,
                               ImVec2(-1, 34), UI::Icon::Folder))
                    openPath(proot);
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            UI::BeginCard("##env_actions", TR("env.actions"), colW, TR("env.actions_desc"),
                          UI::Icon::Sparkle);
            {
                if (UI::Button("##e_init", TR("env.init"), UI::Variant::Solid,
                               ImVec2(-1, 38), UI::Icon::Sparkle))
                {
                    askConfirm(TR("env.init"), TR("overview.hint_init"), TR("env.init"), false,
                               [this]()
                               { startInitProject(); });
                }
                float bw = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
                if (UI::Button("##e_venv", TR("env.venv"), UI::Variant::Ghost, ImVec2(bw, 34)))
                    createVenv(false);
                ImGui::SameLine(0.0f, 8.0f);
                if (UI::Button("##e_recreate", TR("env.venv_del"), UI::Variant::Text,
                               ImVec2(bw, 34), UI::Icon::Refresh))
                {
                    askConfirm(TR("env.venv_del"), TR("env.venv_recreate_body"), TR("env.venv_del"), true,
                               [this]()
                               { createVenv(true); });
                }
                if (UI::Button("##e_cli", TR("env.install_cli"), UI::Variant::Ghost,
                               ImVec2(bw, 34), UI::Icon::Download))
                    startProcess("install CLI", "-m pip install -r requirements.txt", cliRoot());
                ImGui::SameLine(0.0f, 8.0f);
                if (UI::Button("##e_srv", TR("env.install_srv"), UI::Variant::Ghost,
                               ImVec2(bw, 34), UI::Icon::Download))
                    startProcess("install server", "-m pip install -r requirements.txt", serverRoot());
                if (UI::Button("##e_verify", TR("env.verify"), UI::Variant::Soft,
                               ImVec2(-1, 34), UI::Icon::Check))
                    startProcess("verify env", "verify_env.py", proot);
            }
            UI::EndCard();
        },
        [&](float colW)
        {
            UI::BeginCard("##env_config", TR("env.config"), colW, TR("env.config_desc"),
                          UI::Icon::Sliders);
            {
                bool llm = EnvOr(s.envCli, "LLM_RECOGNITION_ENABLED", "False") == "True";
                bool llmDb = EnvOr(s.envCli, "LLM_DB_RECOGNITION", "False") == "True";
                bool autoUpdate = EnvOr(s.envCli, "AUTO_UPDATE_MODEL", "True") == "True";
                UI::KV(TR("env.llm"), llm ? TR("env.llm_on") : TR("env.llm_off"), false);
                UI::KV(TR("env.llm_db"), llmDb ? TR("env.llm_on") : TR("env.llm_off"), false);
                UI::KV(TR("env.auto_update"), autoUpdate ? TR("env.llm_on") : TR("env.llm_off"), false);
                UI::KV(TR("env.model_id"), EnvOr(s.envCli, "MODEL_ID", "37ac").c_str(), true);
                UI::KV(TR("env.pretrained_base"),
                       EnvOr(s.envCli, "PRETRAINED_BASE", "imagenet-resnet18").c_str(), true);
                UI::KV(TR("env.dataset_dir"), datasetDir().c_str(), true);
                UI::KV("LLM 模型", EnvOr(s.envCli, "LLM_MODEL_NAME", "—").c_str(), true);
                ImGui::Dummy(ImVec2(0, 8.0f));
                if (UI::Button("##cfg_env", TR("env.open_env"), UI::Variant::Ghost,
                               ImVec2(-1, 34), UI::Icon::Terminal))
                    openTextFile(cliRoot() + "/.env");
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            UI::BeginCard("##env_components", TR("env.components"), colW, TR("env.components_desc"),
                          UI::Icon::Cube);
            {
                struct Comp
                {
                    const char *id;
                    const char *label;
                    std::string dir;
                };
                Comp comps[] = {
                    {"cli", TR("env.comp_cli"), cli},
                    {"srv", TR("env.comp_srv"), srv},
                    {"web", TR("env.comp_web"), proot + "/src/www"},
                };
                for (auto &c : comps)
                {
                    ImGui::PushID(c.id);
                    std::string envFile = c.dir + "/.env";
                    std::string envExample = c.dir + "/.env.example";
                    bool hasEnv = fileExists(envFile);
                    bool hasExample = fileExists(envExample);

                    ImGui::PushFont(F().bodyBold);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink900));
                    ImGui::TextUnformatted(c.label);
                    ImGui::PopStyleColor();
                    ImGui::PopFont();
                    ImGui::SameLine(0.0f, 8.0f);
                    UI::Badge(hasEnv ? TR("env.env_exists") : TR("env.env_missing"),
                              hasEnv ? UI::Tone::Success : UI::Tone::Warning, true);

                    float bw = (ImGui::GetContentRegionAvail().x - 12.0f) / 3.0f;
                    if (UI::Button("##gen", TR("env.gen"), UI::Variant::Soft, ImVec2(bw, 30), UI::Icon::Download, !hasExample))
                    {
                        if (CopyFileA(envExample.c_str(), envFile.c_str(), FALSE))
                        {
                            addSuccess(std::string("[env] ") + c.dir + " → .env");
                            notify(UI::Tone::Success, TR("env.env_ok"));
                            m_envDirty = true;
                        }
                        else
                        {
                            addError(TR("env.env_fail"));
                            notify(UI::Tone::Danger, TR("env.env_fail"));
                        }
                    }
                    ImGui::SameLine(0.0f, 6.0f);
                    if (UI::Button("##openenv", TR("env.open_env"), UI::Variant::Ghost,
                                   ImVec2(bw, 30)))
                        openTextFile(envFile);
                    ImGui::SameLine(0.0f, 6.0f);
                    if (UI::Button("##del", TR("common.delete"), UI::Variant::Text,
                                   ImVec2(bw, 30), UI::Icon::Trash, !hasEnv))
                    {
                        std::string file = envFile;
                        askConfirm(TR("env.del_title"), TR("env.del_body"), TR("common.delete"), true,
                                   [this, file]()
                                   {
                                       if (DeleteFileA(file.c_str()))
                                       {
                                           addSuccess(TR("env.env_del"));
                                           notify(UI::Tone::Success, TR("env.env_del"));
                                           m_envDirty = true;
                                       }
                                       else
                                       {
                                           addError(TR("env.env_del_fail"));
                                       }
                                   });
                    }
                    ImGui::Dummy(ImVec2(0, 4.0f));
                    UI::Divider();
                    ImGui::Dummy(ImVec2(0, 6.0f));
                    ImGui::PopID();
                }
            }
            UI::EndCard();
        });
}

// ===========================================================================
// 设置
// ===========================================================================
void App::renderSettingsPage()
{
    AppStateSnapshot s = snapshot();
    const float gap = T.cardGap;

    renderTwoColumn(
        [&](float colW)
        {
            UI::BeginCard("##set_lang", TR("settings.language"), colW, TR("settings.language_desc"),
                          UI::Icon::Sliders);
            {
                const char *items[] = {TR("settings.chinese"), TR("settings.english")};
                int current = (LangSys::I().lang() == Lang::Chinese) ? 0 : 1;
                int choice = UI::Segmented("##lang", items, 2, current, 220.0f);
                if (choice != current)
                {
                    LangSys::I().setLang(choice == 0 ? Lang::Chinese : Lang::English);
                    markDirty();
                }
                ImGui::Dummy(ImVec2(0, 4.0f));
                UI::Muted("%s", LangSys::I().lang() == Lang::Chinese
                                    ? "界面语言：中文（简体）"
                                    : "Interface language: English");
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            UI::BeginCard("##set_paths", TR("settings.paths"), colW, TR("settings.paths_desc"),
                          UI::Icon::Folder);
            {
                static char progBuf[512] = "";
                static char projBuf[512] = "";
                std::string out;

                UI::Muted("%s", TR("settings.program_dir"));
                if (pathInput("prog", progBuf, sizeof(progBuf), s.programDir,
                              "选择程序目录", out))
                {
                    {
                        std::lock_guard<std::mutex> lock(m_stateMutex);
                        m_programDir = out;
                    }
                    updateProjectPaths();
                    refreshVenvState();
                    m_envDirty = true;
                }
                ImGui::Dummy(ImVec2(0, 6.0f));

                UI::Muted("%s", TR("settings.project_dir"));
                if (pathInput("proj", projBuf, sizeof(projBuf), s.projectRoot,
                              "选择项目目录", out))
                {
                    {
                        std::lock_guard<std::mutex> lock(m_stateMutex);
                        m_projectRoot = out;
                        m_cliRoot = out + "/src/cli";
                        m_serverRoot = out + "/src/server";
                    }
                    refreshVenvState();
                    m_envDirty = true;
                }
                ImGui::Dummy(ImVec2(0, 6.0f));

                UI::Muted("%s", TR("settings.git_url"));
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##giturl", m_gitUrl, sizeof(m_gitUrl));
                UI::Muted("%s", TR("settings.git_url_desc"));
            }
            UI::EndCard();
        },
        [&](float colW)
        {
            UI::BeginCard("##set_console", TR("settings.console"), colW, TR("settings.console_desc"),
                          UI::Icon::Terminal);
            {
                UI::Muted("%s", TR("settings.log_view"));
                ImGui::Dummy(ImVec2(0, 2.0f));
                const char *modes[] = {TR("settings.colored"), TR("settings.plain")};
                int current = m_consolePlain ? 1 : 0;
                int choice = UI::Segmented("##logmode", modes, 2, current, 220.0f);
                if (choice != current)
                    m_consolePlain = (choice == 1);
                ImGui::Dummy(ImVec2(0, 8.0f));
                UI::Toggle("##autoscroll", &m_autoScroll, TR("settings.auto_scroll"));
                ImGui::Dummy(ImVec2(0, 8.0f));
                UI::Muted("%s", TR("settings.log_height"));
                int pct = (int)(m_logRatio * 100.0f);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##ratio", &pct, 18, 62, "%d%%"))
                    m_logRatio = AcClamp(pct / 100.0f, 0.18f, 0.62f);
                ImGui::Dummy(ImVec2(0, 8.0f));
                if (UI::Button("##showlog", m_consoleOpen ? TR("overview.collapse_console") : TR("overview.open_console"),
                               UI::Variant::Ghost, ImVec2(-1, 34), UI::Icon::Terminal))
                    m_consoleOpen = !m_consoleOpen;
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            UI::BeginCard("##set_system", TR("settings.system"), colW, TR("settings.system_desc"),
                          UI::Icon::Server);
            {
                SYSTEM_INFO si;
                GetSystemInfo(&si);
                MEMORYSTATUSEX ms = {};
                ms.dwLength = sizeof(ms);
                GlobalMemoryStatusEx(&ms);
                double memUsed = (double)(ms.ullTotalPhys - ms.ullAvailPhys);
                double memTotal = (double)ms.ullTotalPhys;

                char root[4] = "C:\\";
                if (s.projectRoot.size() >= 2 && s.projectRoot[1] == ':')
                    root[0] = s.projectRoot[0];
                UI::KV(TR("settings.cpu"), fmt("%u cores", si.dwNumberOfProcessors).c_str(), true);
                UI::KV(TR("settings.mem"),
                       fmt("%s / %s (%.0f%%)",
                           fmt("%.1f GB", memUsed / 1024.0 / 1024.0 / 1024.0).c_str(),
                           fmt("%.1f GB", memTotal / 1024.0 / 1024.0 / 1024.0).c_str(),
                           memTotal > 0 ? 100.0 * memUsed / memTotal : 0.0)
                           .c_str(),
                       true);
                ULARGE_INTEGER freeB = {}, totalB = {};
                if (GetDiskFreeSpaceExA(root, &freeB, &totalB, nullptr))
                {
                    UI::KV(TR("settings.disk"),
                           fmt("%c: %.1f GB free", root[0],
                               (double)freeB.QuadPart / 1024.0 / 1024.0 / 1024.0)
                               .c_str(),
                           true);
                }
                ImGui::Dummy(ImVec2(0, 8.0f));
                if (UI::Button("##openconfig", TR("settings.open_config"), UI::Variant::Ghost,
                               ImVec2(-1, 34), UI::Icon::Sliders))
                    openTextFile(programDir() + "/acui_settings.ini");
            }
            UI::EndCard();
            ImGui::Dummy(ImVec2(0, gap));

            UI::BeginCard("##set_about", TR("settings.about"), colW, nullptr, UI::Icon::Sparkle);
            {
                UI::WrapHint(TR("settings.about_desc"));
                ImGui::Dummy(ImVec2(0, 6.0f));
                UI::Badge("37ACUI · v1.0.0", UI::Tone::Pink, true);
                ImGui::SameLine(0.0f, 6.0f);
                UI::Badge("GPL-3.0", UI::Tone::Neutral, true);
                ImGui::Dummy(ImVec2(0, 6.0f));
                UI::Muted("%s", TR("settings.design"));
            }
            UI::EndCard();
        });
}
