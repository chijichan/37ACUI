#include "components.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace UI
{

namespace
{

const Theme::Tokens &T = Theme::T;
const Theme::Fonts &F() { return Theme::fonts(); }

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

void pushBody() { if (F().body) ImGui::PushFont(F().body); }
void popBody() { if (F().body) ImGui::PopFont(); }
void pushSmall() { if (F().caption) ImGui::PushFont(F().caption); }
void popSmall() { if (F().caption) ImGui::PopFont(); }
void pushBold() { if (F().bodyBold) ImGui::PushFont(F().bodyBold); }
void popBold() { if (F().bodyBold) ImGui::PopFont(); }
void pushHeading() { if (F().heading) ImGui::PushFont(F().heading); }
void popHeading() { if (F().heading) ImGui::PopFont(); }
void pushMono() { if (F().mono) ImGui::PushFont(F().mono); }
void popMono() { if (F().mono) ImGui::PopFont(); }

// ---------------------------------------------------------------------------
// 绘制小工具
// ---------------------------------------------------------------------------
void softShadow(ImDrawList *dl, ImVec2 mn, ImVec2 mx, float rounding, float strength = 1.0f)
{
    // 模拟 CSS box-shadow：0 2px 12px rgba(50,41,49,.06)
    for (int i = 1; i <= 4; i++)
    {
        float e = i * 1.6f;
        float a = (0.030f - i * 0.005f) * strength;
        if (a <= 0.0f)
            continue;
        dl->AddRectFilled(ImVec2(mn.x - e * 0.4f, mn.y - e * 0.2f + 2.0f),
                          ImVec2(mx.x + e * 0.4f, mx.y + e * 0.5f + 2.0f),
                          Theme::withAlpha(IM_COL32(50, 41, 49, 255), a), rounding + e * 0.6f);
    }
}

void ellipsePoints(ImVec2 c, float rx, float ry, int seg, ImVec2 *out)
{
    for (int i = 0; i < seg; i++)
    {
        float a = (float)i / (float)seg * 6.28318530718f;
        out[i] = ImVec2(c.x + cosf(a) * rx, c.y + sinf(a) * ry);
    }
}

void strokeArc(ImDrawList *dl, ImVec2 c, float r, float a0, float a1, ImU32 col, float thick)
{
    const int seg = 20;
    ImVec2 pts[21];
    for (int i = 0; i <= seg; i++)
    {
        float a = a0 + (a1 - a0) * (float)i / (float)seg;
        pts[i] = ImVec2(c.x + cosf(a) * r, c.y + sinf(a) * r);
    }
    dl->AddPolyline(pts, seg + 1, col, 0, thick);
}

} // namespace

// ===========================================================================
// 语义色
// ===========================================================================
ImU32 toneBg(Tone t)
{
    switch (t)
    {
    case Tone::Pink: return T.pink100;
    case Tone::Mint: return T.mintBg;
    case Tone::Success: return T.successBg;
    case Tone::Warning: return T.warningBg;
    case Tone::Danger: return T.dangerBg;
    case Tone::Info: return T.infoBg;
    default: return T.ink100;
    }
}

ImU32 toneFg(Tone t)
{
    switch (t)
    {
    case Tone::Pink: return T.pink700;
    case Tone::Mint: return T.mintInk;
    case Tone::Success: return T.successInk;
    case Tone::Warning: return T.warningInk;
    case Tone::Danger: return T.dangerInk;
    case Tone::Info: return T.infoInk;
    default: return T.ink700;
    }
}

ImU32 toneSolid(Tone t)
{
    switch (t)
    {
    case Tone::Pink: return T.pink500;
    case Tone::Mint: return T.mint;
    case Tone::Success: return T.success;
    case Tone::Warning: return T.warning;
    case Tone::Danger: return T.danger;
    case Tone::Info: return T.info;
    default: return T.ink300;
    }
}

// ===========================================================================
// 图标
// ===========================================================================
void DrawIcon(ImDrawList *dl, Icon icon, ImVec2 c, float s, ImU32 col)
{
    const float h = s * 0.5f;
    const float th = AcMax(1.5f, s * 0.10f);

    switch (icon)
    {
    case Icon::Grid:
    {
        float gap = s * 0.14f;
        float cell = (s - gap) * 0.5f;
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
            {
                ImVec2 p(c.x - h + j * (cell + gap), c.y - h + i * (cell + gap));
                dl->AddRectFilled(p, ImVec2(p.x + cell, p.y + cell), col, cell * 0.34f);
            }
        break;
    }
    case Icon::Cube:
    {
        ImVec2 top(c.x, c.y - h);
        ImVec2 rt(c.x + h * 0.92f, c.y - h * 0.52f);
        ImVec2 mid(c.x, c.y - h * 0.06f);
        ImVec2 lt(c.x - h * 0.92f, c.y - h * 0.52f);
        ImVec2 lb(c.x - h * 0.92f, c.y + h * 0.44f);
        ImVec2 bot(c.x, c.y + h * 0.94f);
        ImVec2 rb(c.x + h * 0.92f, c.y + h * 0.44f);
        dl->AddQuad(top, rt, mid, lt, col, th);
        dl->AddLine(lt, lb, col, th);
        dl->AddLine(lb, bot, col, th);
        dl->AddLine(bot, rb, col, th);
        dl->AddLine(rb, rt, col, th);
        dl->AddLine(mid, bot, col, th);
        dl->AddLine(mid, lt, col, th);
        dl->AddLine(mid, rt, col, th);
        break;
    }
    case Icon::Database:
    {
        float rx = s * 0.40f, ry = s * 0.16f;
        ImVec2 top(c.x, c.y - h * 0.62f);
        ImVec2 bot(c.x, c.y + h * 0.62f);
        ImVec2 pts[28];
        ellipsePoints(top, rx, ry, 24, pts);
        dl->AddPolyline(pts, 24, col, ImDrawFlags_Closed, th);
        dl->AddLine(ImVec2(top.x - rx, top.y), ImVec2(top.x - rx, bot.y), col, th);
        dl->AddLine(ImVec2(top.x + rx, top.y), ImVec2(top.x + rx, bot.y), col, th);
        // 两条储层分隔弧
        for (int k = 1; k <= 2; k++)
        {
            ImVec2 mid(c.x, top.y + (bot.y - top.y) * (k / 3.0f));
            ImVec2 arc[13];
            for (int i = 0; i <= 12; i++)
            {
                float a = 3.14159265f * (float)i / 12.0f;
                arc[i] = ImVec2(mid.x + cosf(a) * rx, mid.y + sinf(a) * ry);
            }
            dl->AddPolyline(arc, 13, col, 0, th);
        }
        break;
    }
    case Icon::Network:
    {
        ImVec2 a(c.x, c.y - h * 0.72f);
        ImVec2 b(c.x - h * 0.78f, c.y + h * 0.62f);
        ImVec2 d(c.x + h * 0.78f, c.y + h * 0.62f);
        dl->AddLine(a, b, col, th * 0.9f);
        dl->AddLine(a, d, col, th * 0.9f);
        dl->AddLine(b, d, col, th * 0.9f);
        dl->AddCircleFilled(a, s * 0.13f, col);
        dl->AddCircleFilled(b, s * 0.13f, col);
        dl->AddCircleFilled(d, s * 0.13f, col);
        break;
    }
    case Icon::Server:
    {
        float w = s * 0.84f, hh = s * 0.34f;
        ImVec2 a(c.x - w * 0.5f, c.y - hh * 1.25f);
        ImVec2 b(c.x + w * 0.5f, c.y - hh * 0.15f);
        ImVec2 d(c.x - w * 0.5f, c.y + hh * 0.15f);
        ImVec2 e(c.x + w * 0.5f, c.y + hh * 1.25f);
        dl->AddRectFilled(a, b, Theme::withAlpha(col, 0.22f), 3.0f);
        dl->AddRect(a, b, col, 3.0f, 0, th * 0.9f);
        dl->AddRectFilled(d, e, Theme::withAlpha(col, 0.22f), 3.0f);
        dl->AddRect(d, e, col, 3.0f, 0, th * 0.9f);
        dl->AddCircleFilled(ImVec2(b.x - s * 0.12f, (a.y + b.y) * 0.5f), s * 0.05f, col);
        dl->AddCircleFilled(ImVec2(e.x - s * 0.12f, (d.y + e.y) * 0.5f), s * 0.05f, col);
        break;
    }
    case Icon::Gear:
    {
        float r1 = s * 0.26f, r2 = s * 0.40f;
        dl->AddCircle(c, r1, col, 20, th);
        for (int i = 0; i < 6; i++)
        {
            float a = (float)i / 6.0f * 6.28318530718f + 0.26f;
            dl->AddLine(ImVec2(c.x + cosf(a) * r1, c.y + sinf(a) * r1),
                        ImVec2(c.x + cosf(a) * r2, c.y + sinf(a) * r2), col, th);
        }
        break;
    }
    case Icon::Sliders:
    {
        float x0 = c.x - h * 0.86f, x1 = c.x + h * 0.86f;
        float y1 = c.y - h * 0.36f, y2 = c.y + h * 0.36f;
        dl->AddLine(ImVec2(x0, y1), ImVec2(x1, y1), col, th);
        dl->AddLine(ImVec2(x0, y2), ImVec2(x1, y2), col, th);
        dl->AddCircleFilled(ImVec2(c.x - h * 0.12f, y1), s * 0.14f, col);
        dl->AddCircleFilled(ImVec2(c.x + h * 0.34f, y2), s * 0.14f, col);
        break;
    }
    case Icon::Terminal:
    {
        ImVec2 a(c.x - h * 0.86f, c.y - h * 0.78f);
        ImVec2 b(c.x + h * 0.86f, c.y + h * 0.78f);
        dl->AddRect(a, b, col, 3.0f, 0, th * 0.9f);
        dl->AddLine(ImVec2(c.x - h * 0.42f, c.y - h * 0.18f), ImVec2(c.x - h * 0.14f, c.y + h * 0.06f), col, th);
        dl->AddLine(ImVec2(c.x - h * 0.14f, c.y + h * 0.06f), ImVec2(c.x - h * 0.42f, c.y + h * 0.32f), col, th);
        dl->AddLine(ImVec2(c.x + h * 0.06f, c.y + h * 0.40f), ImVec2(c.x + h * 0.44f, c.y + h * 0.40f), col, th);
        break;
    }
    case Icon::Folder:
    {
        ImVec2 a(c.x - h * 0.86f, c.y - h * 0.52f);
        ImVec2 b(c.x + h * 0.86f, c.y + h * 0.66f);
        ImVec2 pts[6] = {
            ImVec2(a.x, a.y),
            ImVec2(a.x + s * 0.30f, a.y),
            ImVec2(a.x + s * 0.40f, a.y + s * 0.14f),
            ImVec2(b.x, a.y + s * 0.14f),
            ImVec2(b.x, b.y),
            ImVec2(a.x, b.y)};
        dl->AddPolyline(pts, 6, col, ImDrawFlags_Closed, th * 0.9f);
        break;
    }
    case Icon::Play:
    {
        ImVec2 pts[3] = {ImVec2(c.x - h * 0.5f, c.y - h * 0.7f),
                         ImVec2(c.x + h * 0.72f, c.y),
                         ImVec2(c.x - h * 0.5f, c.y + h * 0.7f)};
        dl->AddTriangleFilled(pts[0], pts[1], pts[2], col);
        break;
    }
    case Icon::Stop:
        dl->AddRectFilled(ImVec2(c.x - h * 0.55f, c.y - h * 0.55f),
                          ImVec2(c.x + h * 0.55f, c.y + h * 0.55f), col, 2.5f);
        break;
    case Icon::Refresh:
    {
        strokeArc(dl, c, s * 0.38f, -0.6f, 4.6f, col, th);
        ImVec2 tip(c.x + cosf(4.6f) * s * 0.38f, c.y + sinf(4.6f) * s * 0.38f);
        dl->AddTriangleFilled(ImVec2(tip.x - s * 0.02f, tip.y - s * 0.18f),
                              ImVec2(tip.x + s * 0.18f, tip.y + s * 0.04f),
                              ImVec2(tip.x - s * 0.16f, tip.y + s * 0.14f), col);
        break;
    }
    case Icon::Rotate:
    {
        strokeArc(dl, c, s * 0.38f, 3.4f, 8.6f, col, th);
        float a = 3.4f;
        ImVec2 tip(c.x + cosf(a) * s * 0.38f, c.y + sinf(a) * s * 0.38f);
        dl->AddTriangleFilled(ImVec2(tip.x + s * 0.02f, tip.y - s * 0.18f),
                              ImVec2(tip.x - s * 0.18f, tip.y + s * 0.04f),
                              ImVec2(tip.x + s * 0.16f, tip.y + s * 0.14f), col);
        break;
    }
    case Icon::Trash:
    {
        float w = s * 0.28f;
        dl->AddLine(ImVec2(c.x - h * 0.86f, c.y - h * 0.56f), ImVec2(c.x + h * 0.86f, c.y - h * 0.56f), col, th);
        dl->AddLine(ImVec2(c.x - w * 0.5f, c.y - h * 0.86f), ImVec2(c.x + w * 0.5f, c.y - h * 0.86f), col, th);
        ImVec2 pts[4] = {ImVec2(c.x - h * 0.62f, c.y - h * 0.50f),
                         ImVec2(c.x + h * 0.62f, c.y - h * 0.50f),
                         ImVec2(c.x + h * 0.44f, c.y + h * 0.80f),
                         ImVec2(c.x - h * 0.44f, c.y + h * 0.80f)};
        dl->AddPolyline(pts, 4, col, ImDrawFlags_Closed, th * 0.9f);
        break;
    }
    case Icon::Cloud:
    {
        float r = s * 0.24f;
        dl->AddCircle(ImVec2(c.x - s * 0.20f, c.y - s * 0.02f), r, col, 18, th);
        dl->AddCircle(ImVec2(c.x + s * 0.16f, c.y - s * 0.06f), r * 0.92f, col, 18, th);
        dl->AddCircle(ImVec2(c.x, c.y + s * 0.10f), r * 1.05f, col, 18, th);
        dl->AddLine(ImVec2(c.x - s * 0.42f, c.y + s * 0.30f), ImVec2(c.x + s * 0.40f, c.y + s * 0.30f), col, th);
        break;
    }
    case Icon::Check:
    {
        ImVec2 pts[3] = {ImVec2(c.x - h * 0.62f, c.y + h * 0.02f),
                         ImVec2(c.x - h * 0.12f, c.y + h * 0.50f),
                         ImVec2(c.x + h * 0.66f, c.y - h * 0.46f)};
        dl->AddPolyline(pts, 3, col, 0, th * 1.15f);
        break;
    }
    case Icon::Info:
        dl->AddCircle(c, s * 0.42f, col, 20, th);
        dl->AddCircleFilled(ImVec2(c.x, c.y - h * 0.32f), s * 0.055f, col);
        dl->AddLine(ImVec2(c.x, c.y - h * 0.08f), ImVec2(c.x, c.y + h * 0.34f), col, th);
        break;
    case Icon::Alert:
    {
        ImVec2 tri[3] = {ImVec2(c.x, c.y - h * 0.86f),
                         ImVec2(c.x + h * 0.92f, c.y + h * 0.72f),
                         ImVec2(c.x - h * 0.92f, c.y + h * 0.72f)};
        dl->AddPolyline(tri, 3, col, ImDrawFlags_Closed, th * 0.95f);
        dl->AddLine(ImVec2(c.x, c.y - h * 0.32f), ImVec2(c.x, c.y + h * 0.16f), col, th);
        dl->AddCircleFilled(ImVec2(c.x, c.y + h * 0.42f), s * 0.055f, col);
        break;
    }
    case Icon::ChevronRight:
        dl->AddLine(ImVec2(c.x - h * 0.24f, c.y - h * 0.5f), ImVec2(c.x + h * 0.24f, c.y), col, th);
        dl->AddLine(ImVec2(c.x + h * 0.24f, c.y), ImVec2(c.x - h * 0.24f, c.y + h * 0.5f), col, th);
        break;
    case Icon::ChevronDown:
        dl->AddLine(ImVec2(c.x - h * 0.5f, c.y - h * 0.22f), ImVec2(c.x, c.y + h * 0.26f), col, th);
        dl->AddLine(ImVec2(c.x, c.y + h * 0.26f), ImVec2(c.x + h * 0.5f, c.y - h * 0.22f), col, th);
        break;
    case Icon::Close:
        dl->AddLine(ImVec2(c.x - h * 0.44f, c.y - h * 0.44f), ImVec2(c.x + h * 0.44f, c.y + h * 0.44f), col, th);
        dl->AddLine(ImVec2(c.x + h * 0.44f, c.y - h * 0.44f), ImVec2(c.x - h * 0.44f, c.y + h * 0.44f), col, th);
        break;
    case Icon::Power:
    {
        strokeArc(dl, c, s * 0.38f, 0.78f, 5.50f, col, th);
        dl->AddLine(ImVec2(c.x, c.y - h * 0.62f), ImVec2(c.x, c.y + h * 0.06f), col, th);
        break;
    }
    case Icon::Sparkle:
    {
        float r = h * 0.94f, r2 = r * 0.26f;
        ImVec2 pts[8];
        for (int i = 0; i < 8; i++)
        {
            float a = (float)i / 8.0f * 6.28318530718f - 1.5707963f;
            float rr = (i % 2 == 0) ? r : r2;
            pts[i] = ImVec2(c.x + cosf(a) * rr, c.y + sinf(a) * rr);
        }
        dl->AddConvexPolyFilled(pts, 8, col);
        break;
    }
    case Icon::Upload:
    case Icon::Download:
    {
        bool up = (icon == Icon::Upload);
        float dir = up ? -1.0f : 1.0f;
        float x0 = c.x - h * 0.6f, x1 = c.x + h * 0.6f;
        dl->AddLine(ImVec2(c.x, c.y + dir * h * 0.66f), ImVec2(c.x, c.y - dir * h * 0.40f), col, th);
        dl->AddLine(ImVec2(c.x - h * 0.36f, c.y - dir * h * 0.06f), ImVec2(c.x, c.y - dir * h * 0.44f), col, th);
        dl->AddLine(ImVec2(c.x + h * 0.36f, c.y - dir * h * 0.06f), ImVec2(c.x, c.y - dir * h * 0.44f), col, th);
        dl->AddLine(ImVec2(x0, c.y + h * 0.82f), ImVec2(x1, c.y + h * 0.82f), col, th);
        break;
    }
    default:
        break;
    }
}

void IconBox(Icon icon, float box, Tone tone, float iconScale)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + box, p.y + box), toneBg(tone), box * 0.32f);
    DrawIcon(dl, icon, ImVec2(p.x + box * 0.5f, p.y + box * 0.5f), box * iconScale, toneFg(tone));
    ImGui::Dummy(ImVec2(box, box));
}

// ===========================================================================
// 按钮
// ===========================================================================
bool Button(const char *id, const char *label, Variant v, ImVec2 size, Icon icon,
            bool disabled, const char *tooltip)
{
    if (!label)
        label = "";

    // ---- 计算尺寸 ----
    pushBold();
    ImVec2 ts = ImGui::CalcTextSize(label);
    popBold();
    float h = size.y > 0.0f ? size.y : 38.0f;
    float padX = (v == Variant::Text) ? 10.0f : (h <= 30.0f ? 12.0f : 18.0f);
    float iconS = AcMin(16.0f, h * 0.44f);
    float contentW = ts.x + (icon != Icon::None ? iconS + 7.0f : 0.0f);
    float w = size.x > 0.0f ? size.x : (contentW + padX * 2.0f);
    if (size.x < 0.0f)
        w = ImGui::GetContentRegionAvail().x;

    // ---- 交互 ----
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(id, ImVec2(w, h)); // 鼠标与键盘导航均在此触发
    ImGuiID wid = ImGui::GetItemID();

    bool hovered = ImGui::IsItemHovered() && !disabled;
    bool held = ImGui::IsItemActive() && !disabled;
    bool clicked = pressed && !disabled;

    float ht = Theme::anim(wid, hovered ? 1.0f : 0.0f, 16.0f);
    float pt = Theme::anim(wid ^ 0x9E3779B9u, held ? 1.0f : 0.0f, 26.0f);
    float press = Theme::easeOut(pt) * 1.5f; // 按下时轻微下沉

    // ---- 配色 ----
    ImU32 bg = IM_COL32(0, 0, 0, 0);
    ImU32 fg = T.ink700;
    ImU32 border = IM_COL32(0, 0, 0, 0);
    bool drawBorder = false;
    bool shadowed = false;
    ImU32 shadowCol = 0;

    switch (v)
    {
    case Variant::Solid:
        bg = Theme::mix(T.pink600, T.pink500, ht);
        if (held)
            bg = T.pink700;
        fg = IM_COL32(255, 255, 255, 255);
        shadowed = ht > 0.01f;
        shadowCol = Theme::withAlpha(IM_COL32(201, 52, 112, 255), 0.30f * ht);
        break;
    case Variant::Soft:
        bg = Theme::mix(T.pink100, T.pink200, ht);
        if (held)
            bg = T.pink300;
        fg = T.pink700;
        break;
    case Variant::Ghost:
        bg = Theme::mix(IM_COL32(255, 255, 255, 0), T.pink50, ht);
        if (held)
            bg = T.pink100;
        fg = Theme::mix(T.ink700, T.pink600, ht);
        border = Theme::mix(T.ink100, T.pink300, ht);
        drawBorder = true;
        break;
    case Variant::Danger:
        bg = Theme::mix(T.danger, IM_COL32(192, 51, 51, 255), ht);
        if (held)
            bg = IM_COL32(168, 42, 42, 255);
        fg = IM_COL32(255, 255, 255, 255);
        shadowed = ht > 0.01f;
        shadowCol = Theme::withAlpha(T.danger, 0.28f * ht);
        break;
    case Variant::Text:
        bg = Theme::mix(IM_COL32(255, 255, 255, 0), T.pink50, ht);
        fg = Theme::mix(T.ink700, T.pink600, ht);
        break;
    }

    if (disabled)
    {
        bg = Theme::withAlpha(bg, 0.55f);
        fg = Theme::withAlpha(fg, 0.45f);
        border = Theme::withAlpha(border, 0.5f);
        shadowed = false;
    }

    // ---- 绘制 ----
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 mn(p.x, p.y + press), mx(p.x + w, p.y + h + press);
    if (shadowed)
        dl->AddRectFilled(ImVec2(mn.x + 1, mn.y + 3), ImVec2(mx.x - 1, mx.y + 4), shadowCol, h * 0.5f);

    if (bg != 0 && (bg >> IM_COL32_A_SHIFT) != 0)
        dl->AddRectFilled(mn, mx, bg, h * 0.5f);
    if (drawBorder)
        dl->AddRect(mn, mx, border, h * 0.5f, 0, 1.4f);

    // 内容：图标 + 文案
    float cx = mn.x + (w - contentW) * 0.5f;
    float cy = mn.y + h * 0.5f;
    if (icon != Icon::None)
    {
        DrawIcon(dl, icon, ImVec2(cx + iconS * 0.5f, cy), iconS, fg);
        cx += iconS + 7.0f;
    }
    {
        ImFont *bf = F().bodyBold ? F().bodyBold : F().body;
        float fs = bf ? bf->LegacySize : ImGui::GetFontSize();
        ImVec2 labelSize = bf ? bf->CalcTextSizeA(fs, FLT_MAX, 0.0f, label)
                              : ImGui::CalcTextSize(label);
        dl->AddText(bf, fs, ImVec2(cx, cy - labelSize.y * 0.5f), fg, label);
    }

    if (tooltip && hovered)
        ImGui::SetTooltip("%s", tooltip);

    return clicked;
}

// ===========================================================================
// 徽章 / 状态
// ===========================================================================
void Badge(const char *text, Tone tone, bool mini)
{
    if (!text || !text[0])
        return;
    ImFont *font = F().caption ? F().caption : F().body;
    ImGui::PushFont(font);
    ImVec2 ts = ImGui::CalcTextSize(text);
    ImGui::PopFont();

    float h = mini ? 18.0f : 21.0f;
    float padX = mini ? 8.0f : 10.0f;
    float lineH = ImGui::GetTextLineHeight();

    ImVec2 p = ImGui::GetCursorScreenPos();
    float y = p.y + (lineH - h) * 0.5f;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + ts.x + padX * 2, y + h), toneBg(tone), h * 0.5f);

    ImVec2 after = ImGui::GetCursorScreenPos();
    ImGui::PushFont(font);
    ImGui::SetCursorScreenPos(ImVec2(p.x + padX, y + (h - ImGui::GetTextLineHeight()) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(toneFg(tone)));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::SetCursorScreenPos(after);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(ts.x + padX * 2, lineH));
}

void StatusDot(Tone tone, float radius, bool halo)
{
    float lineH = ImGui::GetTextLineHeight();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 c(p.x + radius + 1.0f, p.y + lineH * 0.5f);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    if (halo)
        dl->AddCircleFilled(c, radius * 2.1f, Theme::withAlpha(toneSolid(tone), 0.16f));
    dl->AddCircleFilled(c, radius, toneSolid(tone));
    ImGui::Dummy(ImVec2(radius * 2 + 6.0f, lineH));
}

void StatusPill(const char *text, Tone tone, bool dot)
{
    ImFont *font = F().caption ? F().caption : F().body;
    ImGui::PushFont(font);
    ImVec2 ts = ImGui::CalcTextSize(text ? text : "");
    ImGui::PopFont();

    float h = 22.0f;
    float padX = 9.0f;
    float dotW = dot ? 12.0f : 0.0f;
    float lineH = ImGui::GetTextLineHeight();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float y = p.y + (lineH - h) * 0.5f;
    float w = ts.x + padX * 2 + dotW;

    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + w, y + h), toneBg(tone), h * 0.5f);
    if (dot)
        dl->AddCircleFilled(ImVec2(p.x + padX + 3.0f, y + h * 0.5f), 3.4f, toneSolid(tone));

    ImVec2 after = ImGui::GetCursorScreenPos();
    ImGui::PushFont(font);
    ImGui::SetCursorScreenPos(ImVec2(p.x + padX + dotW, y + (h - ImGui::GetTextLineHeight()) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(toneFg(tone)));
    ImGui::TextUnformatted(text ? text : "");
    ImGui::PopStyleColor();
    ImGui::SetCursorScreenPos(after);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(w, lineH));
}

// ===========================================================================
// 排版
// ===========================================================================
namespace
{
void textV(const char *fmt, va_list ap, ImFont *font, ImU32 color, float wrap)
{
    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    if (font)
        ImGui::PushFont(font);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(color));
    if (wrap > 0.0f)
        ImGui::PushTextWrapPos(wrap);
    else
        ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(buf);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    if (font)
        ImGui::PopFont();
}
} // namespace

void Muted(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    textV(fmt, ap, F().caption, T.ink500, 0.0f);
    va_end(ap);
}

void Text(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    textV(fmt, ap, F().body, T.ink700, 0.0f);
    va_end(ap);
}

void Strong(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    textV(fmt, ap, F().bodyBold, T.ink900, 0.0f);
    va_end(ap);
}

void SpacerY(float h) { ImGui::Dummy(ImVec2(0.0f, h)); }

void Divider()
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + w, p.y + 1.0f), T.ink100);
    ImGui::Dummy(ImVec2(w, 1.0f));
}

void SectionLabel(const char *text)
{
    if (!text)
        return;
    ImGui::PushFont(F().caption);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink500));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void WrapHint(const char *text)
{
    if (!text)
        return;
    ImGui::PushFont(F().caption);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink500));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

// ===========================================================================
// 卡片
// ===========================================================================
namespace
{
struct CardCtx
{
    ImVec2 pos;
    float width;
    bool header;
};
std::vector<CardCtx> g_cards;
} // namespace

bool BeginCard(const char *id, const char *title, float width, const char *subtitle,
               Icon icon, const char *actionLabel, bool *actionClicked)
{
    if (actionClicked)
        *actionClicked = false;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, T.radiusCard);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.surface);
    ImGui::PushStyleColor(ImGuiCol_Border, T.ink100);

    float w = width;
    if (w <= 0.0f)
        w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool open = ImGui::BeginChild(id, ImVec2(w, 0.0f),
                                  ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
                                  ImGuiWindowFlags_NoScrollbar);

    bool hasHeader = (title && title[0]) || icon != Icon::None;
    if (hasHeader && open)
    {
        ImVec2 headerPos = ImGui::GetCursorScreenPos();
        float contentW = ImGui::GetContentRegionAvail().x;

        // 右上角动作按钮
        if (actionLabel && actionLabel[0])
        {
            pushBold();
            ImVec2 ats = ImGui::CalcTextSize(actionLabel);
            popBold();
            float aw = ats.x + 20.0f;
            std::string actId = std::string(id) + "_act";
            ImGui::SetCursorScreenPos(ImVec2(headerPos.x + contentW - aw, headerPos.y + 1.0f));
            if (Button(actId.c_str(), actionLabel, Variant::Text, ImVec2(aw, 26.0f)))
            {
                if (actionClicked)
                    *actionClicked = true;
            }
            ImGui::SetCursorScreenPos(headerPos);
        }

        float box = 30.0f;
        if (icon != Icon::None)
        {
            IconBox(icon, box, Tone::Pink, 0.50f);
            ImGui::SameLine(0, 11);
        }

        ImGui::BeginGroup();
        if (title && title[0])
        {
            ImGui::PushFont(F().heading);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink900));
            ImGui::TextUnformatted(title);
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }
        if (subtitle && subtitle[0])
        {
            ImGui::PushFont(F().caption);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink500));
            ImGui::TextUnformatted(subtitle);
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }
        ImGui::EndGroup();

        // 头部下缘分隔线，固定与标题块底部对齐
        float bottom = headerPos.y + box;
        if (ImGui::GetCursorScreenPos().y > bottom)
            bottom = ImGui::GetCursorScreenPos().y;
        ImGui::SetCursorScreenPos(ImVec2(headerPos.x, bottom + 8.0f));
        Divider();
        ImGui::Dummy(ImVec2(0, 8.0f));
    }

    g_cards.push_back({p, w, hasHeader});
    return open;
}

void EndCard()
{
    if (!g_cards.empty())
        g_cards.pop_back();

    // 末尾补一个零尺寸 item：最后一项若通过 SetCursorScreenPos 定位，
    // 子窗口结束时 ImGui 会断言「extend window boundaries」。
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::EndChild();
    ImVec2 mn = ImGui::GetItemRectMin();
    ImVec2 mx = ImGui::GetItemRectMax();
    softShadow(ImGui::GetWindowDrawList(), mn, mx, T.radiusCard);

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

// ===========================================================================
// 表单
// ===========================================================================
bool Toggle(const char *id, bool *value, const char *label)
{
    if (!value)
        return false;
    float h = 22.0f;
    float w = 40.0f;
    bool changed = false;

    ImGui::PushID(id);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##t", ImVec2(w, h));
    if (ImGui::IsItemClicked())
    {
        *value = !*value;
        changed = true;
    }
    float t = Theme::anim(ImGui::GetItemID(), *value ? 1.0f : 0.0f, 18.0f);
    ImU32 track = Theme::mix(T.ink300, T.pink600, t);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), track, h * 0.5f);
    float kr = h * 0.5f - 3.0f;
    float kx = p.x + 3.0f + kr + (w - 6.0f - kr * 2.0f) * t;
    dl->AddCircleFilled(ImVec2(kx, p.y + h * 0.5f), kr, IM_COL32(255, 255, 255, 255));

    if (label && label[0])
    {
        ImFont *bf = F().body;
        float fs = bf ? bf->LegacySize : ImGui::GetFontSize();
        ImVec2 labelSize = bf ? bf->CalcTextSizeA(fs, FLT_MAX, 0.0f, label)
                              : ImGui::CalcTextSize(label);
        dl->AddText(bf, fs, ImVec2(p.x + w + 10.0f, p.y + (h - labelSize.y) * 0.5f),
                    T.ink700, label);
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::PopID();
    return changed;
}

int Segmented(const char *id, const char *const *items, int count, int current, float width)
{
    if (count <= 0)
        return current;

    const float h = 36.0f;
    const float pad = 4.0f;
    pushBody();
    float totalText = 0.0f;
    for (int i = 0; i < count; i++)
        totalText += ImGui::CalcTextSize(items[i]).x;
    popBody();

    /// 依据文本宽度按比例分配；整体宽度不足时给最小值
    float minW = 64.0f;
    float need = totalText + (float)count * 28.0f + pad * 2.0f;
    float w = width > 0.0f ? width : need;
    float avail = ImGui::GetContentRegionAvail().x;
    if (w > avail)
        w = avail;
    if (w < minW * count)
        w = minW * count;

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), T.surface2, h * 0.5f);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), T.ink100, h * 0.5f, 0, 1.0f);

    float inner = w - pad * 2.0f;
    float segW = inner / (float)count;
    int result = current;

    for (int i = 0; i < count; i++)
    {
        ImGui::PushID(i);
        ImVec2 sp(p.x + pad + segW * i, p.y + pad);
        ImVec2 ssz(segW, h - pad * 2.0f);
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h)); // 占位，避免影响父布局
        ImGui::SetCursorScreenPos(sp);
        bool clicked = ImGui::InvisibleButton("##seg", ssz);
        bool hovered = ImGui::IsItemHovered();
        bool active = (i == result);
        float at = Theme::anim(ImGui::GetItemID(), (active || hovered) ? 1.0f : 0.0f, 16.0f);

        if (active)
        {
            dl->AddRectFilled(ImVec2(sp.x + 1, sp.y), ImVec2(sp.x + ssz.x - 1, sp.y + ssz.y),
                              T.surface, ssz.y * 0.5f);
            dl->AddRect(ImVec2(sp.x + 1, sp.y), ImVec2(sp.x + ssz.x - 1, sp.y + ssz.y),
                        Theme::withAlpha(T.pink200, 0.75f), ssz.y * 0.5f, 0, 1.0f);
        }
        else if (hovered)
        {
            dl->AddRectFilled(sp, ImVec2(sp.x + ssz.x, sp.y + ssz.y),
                              Theme::withAlpha(T.surface, 0.55f * at), ssz.y * 0.5f);
        }

        ImU32 col = active ? T.pink600 : Theme::mix(T.ink500, T.ink700, at);
        pushBody();
        ImVec2 tsz = ImGui::CalcTextSize(items[i]);
        ImVec2 after = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(sp.x + (ssz.x - tsz.x) * 0.5f,
                                         sp.y + (ssz.y - ImGui::GetTextLineHeight()) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(col));
        ImGui::TextUnformatted(items[i]);
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(after);
        popBody();

        if (clicked && i != result)
            result = i;
        ImGui::PopID();
    }

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    ImGui::Dummy(ImVec2(w, 0.0f));
    return result;
}

void ProgressBar(float t, const char *label)
{
    t = clamp01(t);
    if (label && label[0])
        Muted("%s", label);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    const float h = 8.0f;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), T.ink100, h * 0.5f);
    if (t > 0.0f)
        dl->AddRectFilled(p, ImVec2(p.x + AcMax(8.0f, w * t), p.y + h), T.pink500, h * 0.5f);
    ImGui::Dummy(ImVec2(w, h));
}

void KV(const char *key, const char *value, bool mono, float keyWidth)
{
    if (!value)
        value = "";
    float x0 = ImGui::GetCursorPosX();
    ImGui::PushFont(F().caption);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink500));
    ImGui::TextUnformatted(key ? key : "");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::SameLine(x0 + keyWidth);

    if (mono)
        pushMono();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink700));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(value);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    if (mono)
        popMono();
}

void EmptyState(Icon icon, const char *title, const char *desc)
{
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(w, 8.0f));
    float cx = ImGui::GetCursorScreenPos().x + w * 0.5f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    float box = 52.0f;
    dl->AddRectFilled(ImVec2(cx - box * 0.5f, p.y), ImVec2(cx + box * 0.5f, p.y + box),
                      T.surface2, 16.0f);
    DrawIcon(dl, icon, ImVec2(cx, p.y + box * 0.5f), 24.0f, T.ink300);
    ImGui::Dummy(ImVec2(w, box + 10.0f));

    if (title && title[0])
    {
        ImGui::PushFont(F().bodyBold);
        ImVec2 ts = ImGui::CalcTextSize(title);
        ImGui::SetCursorScreenPos(ImVec2(cx - ts.x * 0.5f, ImGui::GetCursorScreenPos().y));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink700));
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
    if (desc && desc[0])
    {
        ImGui::PushFont(F().caption);
        ImGui::PushTextWrapPos(cx + w * 0.32f);
        ImVec2 ts = ImGui::CalcTextSize(desc, nullptr, false, w * 0.64f);
        ImGui::SetCursorScreenPos(ImVec2(cx - AcMin(ts.x, w * 0.64f) * 0.5f, ImGui::GetCursorScreenPos().y + 2.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(T.ink500));
        ImGui::TextUnformatted(desc);
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
    }
    ImGui::Dummy(ImVec2(w, 8.0f));
}

// ===========================================================================
// Toast
// ===========================================================================
namespace
{
struct ToastItem
{
    Tone tone;
    std::string text;
    double born;
};
std::vector<ToastItem> g_toasts;
const float kToastW = 300.0f;
} // namespace

void Notify(Tone tone, const std::string &text)
{
    g_toasts.push_back({tone, text, ImGui::GetTime()});
    if (g_toasts.size() > 4)
        g_toasts.erase(g_toasts.begin());
}

void RenderToasts()
{
    if (g_toasts.empty())
        return;

    const double now = ImGui::GetTime();
    const float life = 3.4f;
    g_toasts.erase(std::remove_if(g_toasts.begin(), g_toasts.end(),
                                  [&](const ToastItem &t) { return now - t.born > life; }),
                   g_toasts.end());
    if (g_toasts.empty())
        return;

    Theme::markAnimating(); // 有 Toast 时保持 60fps，让淡入淡出连贯

    ImGuiIO &io = ImGui::GetIO();
    const float stackW = 320.0f;     // 右上角通知栈的可用宽度
    const float padX = 14.0f;        // 卡片内边距
    const float padY = 11.0f;
    const float iconS = 15.0f;       // 语义图标尺寸（不画圆底/彩条，避免“AI 助手”既视感）
    const float iconGap = 10.0f;
    const float minW = 132.0f;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - stackW - 16.0f, 14.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::Begin("##toasts", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoBackground);

    for (const ToastItem &t : g_toasts)
    {
        const float age = (float)(now - t.born);
        const float in = clamp01(age / 0.18f);
        const float out = clamp01((life - age) / 0.35f);
        const float a = Theme::easeOut(in) * out;
        const float rise = (1.0f - Theme::easeOut(in)) * 6.0f; // 轻微上浮，不做横向滑入

        ImGui::PushFont(F().body);
        ImVec2 ts = ImGui::CalcTextSize(t.text.c_str(), nullptr, false, stackW - 52.0f);
        ImGui::PopFont();

        float cardW = ts.x + padX * 2.0f + iconS + iconGap;
        if (cardW < minW)
            cardW = minW;
        if (cardW > stackW)
            cardW = stackW;
        const float cardH = AcMax(44.0f, ts.y + padY * 2.0f);

        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 mn(p.x + (stackW - cardW), p.y - rise);
        ImVec2 mx(mn.x + cardW, mn.y + cardH);
        ImDrawList *dl = ImGui::GetWindowDrawList();

        for (int i = 1; i <= 3; i++)
        {
            float e = (float)i * 1.8f;
            dl->AddRectFilled(ImVec2(mn.x - e * 0.3f, mn.y + 2.0f),
                              ImVec2(mx.x + e * 0.3f, mx.y + e * 0.5f + 2.0f),
                              Theme::withAlpha(IM_COL32(50, 41, 49, 255), 0.035f * a),
                              T.radiusInput + e * 0.5f);
        }
        dl->AddRectFilled(mn, mx, Theme::withAlpha(T.surface, a), T.radiusInput);
        dl->AddRect(mn, mx, Theme::withAlpha(T.ink100, a), T.radiusInput, 0, 1.0f);

        Icon icn = Icon::Info;
        if (t.tone == Tone::Danger)
            icn = Icon::Close;
        else if (t.tone == Tone::Warning)
            icn = Icon::Alert;
        else if (t.tone == Tone::Success)
            icn = Icon::Check;
        DrawIcon(dl, icn, ImVec2(mn.x + padX + iconS * 0.5f, mn.y + cardH * 0.5f), iconS,
                 Theme::withAlpha(toneFg(t.tone), a));

        ImVec2 after = ImGui::GetCursorScreenPos();
        ImGui::PushFont(F().body);
        ImGui::SetCursorScreenPos(
            ImVec2(mn.x + padX + iconS + iconGap, mn.y + (cardH - ts.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImGui::ColorConvertU32ToFloat4(Theme::withAlpha(T.ink900, a)));
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(t.text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(after);

        ImGui::Dummy(ImVec2(stackW, cardH));
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}
void DimBackground()
{
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize,
                                                  IM_COL32(50, 41, 49, 90));
}

} // namespace UI
