#pragma once

// ============================================================================
// 37ACUI 设计系统「樱花拿铁」
//
// 令牌与 37AC 前端（src/www/public/static/css/ac-tokens.css）一一对应：
//   · 唯一强调色：樱花粉 --ac-pink-*
//   · 主文字暖灰黑 --ac-ink-900，禁纯黑
//   · 形状锁定：按钮/徽章 pill，输入 12，卡片 16，模态 20
//   · 阴影柔光带粉调，交互回弹曲线 150~250ms
// 桌面端用 Segoe UI / 微软雅黑 / Cascadia Mono 替代 Web 端的
// Nunito / JetBrains Mono（woff2 无法被 stb_truetype 直接加载）。
// ============================================================================

#include "imgui.h"

// ImGui 的 ImMax/ImMin/ImClamp 位于 imgui_internal.h；这里提供同名语义的小工具，
// 避免业务代码依赖内部头文件。
template <typename T>
inline T AcMax(T a, T b) { return a > b ? a : b; }
template <typename T>
inline T AcMin(T a, T b) { return a < b ? a : b; }
template <typename T>
inline T AcClamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

namespace Theme
{

struct Tokens
{
    // 基底
    ImU32 bg = IM_COL32(253, 249, 251, 255);      // #FDF9FB
    ImU32 surface = IM_COL32(255, 255, 255, 255); // #FFFFFF
    ImU32 surface2 = IM_COL32(251, 243, 247, 255); // #FBF3F7

    // 文字（暖灰，禁纯黑）
    ImU32 ink900 = IM_COL32(50, 41, 49, 255);  // #322931
    ImU32 ink700 = IM_COL32(87, 75, 84, 255);  // #574B54
    ImU32 ink500 = IM_COL32(138, 124, 133, 255); // #8A7C85
    ImU32 ink300 = IM_COL32(201, 189, 197, 255); // #C9BDC5
    ImU32 ink100 = IM_COL32(239, 232, 236, 255); // #EFE8EC

    // 樱花粉（唯一强调色）
    ImU32 pink50 = IM_COL32(253, 240, 246, 255);
    ImU32 pink100 = IM_COL32(251, 224, 238, 255);
    ImU32 pink200 = IM_COL32(246, 194, 220, 255);
    ImU32 pink300 = IM_COL32(240, 156, 195, 255);
    ImU32 pink400 = IM_COL32(232, 111, 166, 255);
    ImU32 pink500 = IM_COL32(222, 79, 141, 255);
    ImU32 pink600 = IM_COL32(201, 52, 112, 255); // 白字对比度 5:1
    ImU32 pink700 = IM_COL32(168, 42, 92, 255);

    // 点缀薄荷青（仅小面积）
    ImU32 mint = IM_COL32(61, 189, 180, 255);
    ImU32 mintBg = IM_COL32(223, 245, 243, 255);
    ImU32 mintInk = IM_COL32(28, 111, 105, 255);

    // 语义色（浅底 + 深字，全部过 WCAG AA）
    ImU32 success = IM_COL32(47, 163, 107, 255);
    ImU32 successBg = IM_COL32(229, 245, 237, 255);
    ImU32 successInk = IM_COL32(27, 122, 75, 255);
    ImU32 warning = IM_COL32(224, 138, 30, 255);
    ImU32 warningBg = IM_COL32(252, 240, 222, 255);
    ImU32 warningInk = IM_COL32(138, 83, 16, 255);
    ImU32 danger = IM_COL32(214, 69, 69, 255);
    ImU32 dangerBg = IM_COL32(251, 231, 231, 255);
    ImU32 dangerInk = IM_COL32(179, 39, 39, 255);
    ImU32 info = IM_COL32(62, 143, 217, 255);
    ImU32 infoBg = IM_COL32(229, 241, 251, 255);
    ImU32 infoInk = IM_COL32(42, 111, 168, 255);

    // 形状（Shape Lock）
    float radiusPill = 999.0f;
    float radiusInput = 12.0f;
    float radiusCard = 16.0f;
    float radiusModal = 20.0f;

    // 动效
    float durFast = 0.15f;
    float dur = 0.22f;

    // 布局
    float sidebarW = 232.0f;
    float pagePad = 26.0f;
    float cardGap = 16.0f;
    float statusBarH = 30.0f;
    float minConsoleH = 120.0f;
};

extern Tokens T;

// 字体集合（Nunito→Segoe UI，JetBrains Mono→Cascadia Mono 的桌面替代）
struct Fonts
{
    ImFont *body = nullptr;     // 正文 16
    ImFont *bodyBold = nullptr; // 正文加粗 16（按钮/重点）
    ImFont *heading = nullptr;  // 标题 18 粗
    ImFont *display = nullptr;  // 品牌/大数字 26 粗
    ImFont *caption = nullptr;  // 辅助 13（Windows 头文件有 small 宏，故命名 caption）
    ImFont *mono = nullptr;     // 等宽 15（日志/数值）
};

void setFonts(const Fonts &f);
const Fonts &fonts();
Fonts loadFonts();

// 释放字体文件缓存（须在销毁 ImGui 上下文之后调用）。
// 隐藏到系统托盘时调用，把字体占用的内存全部还给系统。
void releaseFontData();

// 应用全局 ImGui 样式（颜色 + 圆角 + 间距）
void applyStyle();

// ---- 颜色工具 ----
ImU32 withAlpha(ImU32 c, float a);
ImU32 mix(ImU32 a, ImU32 b, float t); // t=0 → a
ImU32 lighten(ImU32 c, float f);

// ---- 动效 ----
float easeOut(float t);
float easeSpring(float t); // 回弹曲线 cubic-bezier(.34,1.56,.64,1) 的近似
void beginFrame();         // 每帧开始重置动画状态
bool animating();          // 本帧是否仍有动画在跑（事件驱动循环据此保持 60fps）
float anim(ImGuiID id, float target, float speed = 14.0f); // 平滑趋近 target
void markAnimating();                                     // 主动声明「本帧有动画」（如 Toast）

} // namespace Theme
