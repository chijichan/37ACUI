#pragma once

// ============================================================================
// 37ACUI 组件库
// 与 37AC Web 端 ac-components.css 对齐：胶囊按钮、浅底徽章、16px 卡片、
// 20px 模态、柔光阴影、回弹动效。桌面端用自绘实现以摆脱 ImGui 默认观感。
// ============================================================================

#include "imgui.h"
#include "theme.h"
#include <string>

namespace UI
{

// ---------------------------------------------------------------------------
// 语义色
// ---------------------------------------------------------------------------
enum class Tone
{
    Neutral,
    Pink,
    Mint,
    Success,
    Warning,
    Danger,
    Info
};

ImU32 toneBg(Tone t);    // 浅色底
ImU32 toneFg(Tone t);    // 深色文字（过 WCAG AA）
ImU32 toneSolid(Tone t); // 实色（圆点/进度条）

// ---------------------------------------------------------------------------
// 图标（用图元绘制，避免引入图标字体）
// ---------------------------------------------------------------------------
enum class Icon
{
    None,
    Grid,
    Cube,
    Database,
    Network,
    Server,
    Gear,
    Sliders,
    Terminal,
    Folder,
    Play,
    Stop,
    Refresh,
    Trash,
    Rotate,
    Cloud,
    Check,
    Info,
    ChevronRight,
    ChevronDown,
    Close,
    Power,
    Sparkle,
    Upload,
    Download
};

void DrawIcon(ImDrawList *dl, Icon icon, ImVec2 center, float size, ImU32 color);

// 带浅色底的方形图标块（卡片标题 / 导航）
void IconBox(Icon icon, float box, Tone tone = Tone::Pink, float iconScale = 0.52f);

// ---------------------------------------------------------------------------
// 按钮
// ---------------------------------------------------------------------------
enum class Variant
{
    Solid,  // 樱花粉实心（主操作）
    Soft,   // 粉底深字（次操作）
    Ghost,  // 描边（普通操作）
    Danger, // 危险操作
    Text    // 纯文字
};

// size.x: 0=自适应，<0=占满可用宽度；size.y: 0=默认 38
bool Button(const char *id, const char *label, Variant v = Variant::Soft,
            ImVec2 size = ImVec2(0, 0), Icon icon = Icon::None,
            bool disabled = false, const char *tooltip = nullptr);

// ---------------------------------------------------------------------------
// 徽章 / 状态
// ---------------------------------------------------------------------------
void Badge(const char *text, Tone tone = Tone::Neutral, bool mini = false);
void StatusDot(Tone tone = Tone::Success, float radius = 4.0f, bool halo = true);
void StatusPill(const char *text, Tone tone, bool dot = true);

// ---------------------------------------------------------------------------
// 排版
// ---------------------------------------------------------------------------
void Muted(const char *fmt, ...);  // 小字 ink500
void Text(const char *fmt, ...);   // 正文 ink700
void Strong(const char *fmt, ...); // 加粗 ink900
void SpacerY(float h);
void Divider();
void SectionLabel(const char *text); // 侧栏分组标题
void WrapHint(const char *text);     // 自动换行的浅色说明

// ---------------------------------------------------------------------------
// 卡片
// 返回 false 表示内容不可见（仍必须调用 EndCard）
// ---------------------------------------------------------------------------
bool BeginCard(const char *id, const char *title = nullptr, float width = 0.0f,
               const char *subtitle = nullptr, Icon icon = Icon::None,
               const char *actionLabel = nullptr, bool *actionClicked = nullptr);
void EndCard();

// ---------------------------------------------------------------------------
// 表单
// ---------------------------------------------------------------------------
bool Toggle(const char *id, bool *value, const char *label = nullptr);
int Segmented(const char *id, const char *const *items, int count, int current,
              float width = 0.0f);
void ProgressBar(float t, const char *label = nullptr);
void KV(const char *key, const char *value, bool mono = false, float keyWidth = 96.0f);
void EmptyState(Icon icon, const char *title, const char *desc);

// ---------------------------------------------------------------------------
// Toast 通知（右上角堆叠，纯文本）
// ---------------------------------------------------------------------------
void Notify(Tone tone, const std::string &text);
void RenderToasts();

// 模态框背后的蒙层（rgba(50,41,49,.35)）
void DimBackground();

} // namespace UI
