#include "theme.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <ctime>
#include <unordered_map>
#include <vector>

namespace Theme
{

Tokens T;

// ---------------------------------------------------------------------------
// 字体
// ---------------------------------------------------------------------------
namespace
{
Fonts g_fonts;

// 字体文件缓存：同一路径只读入内存一次。
// ImGui 1.92 的字形按需烘焙，但每个 AddFontFromFileTTF 都会把整个 TTF 再读一份
// （中文雅黑约 20MB，多字号时内存会翻几倍）。这里改为自己读文件、多个字号共享
// 同一块数据，并设置 FontDataOwnedByAtlas=false，让 1.92.6+ 不再复制。
struct FontBlob
{
    std::string path;
    std::vector<char> data;
};

std::vector<FontBlob> g_fontBlobs;

bool fileExists(const char *p)
{
    FILE *f = fopen(p, "rb");
    if (!f)
        return false;
    fclose(f);
    return true;
}

const std::vector<char> *fontBlob(const char *path)
{
    if (!path || !path[0])
        return nullptr;
    for (const FontBlob &b : g_fontBlobs)
        if (b.path == path)
            return &b.data;

    FILE *f = fopen(path, "rb");
    if (!f)
        return nullptr;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0)
    {
        fclose(f);
        return nullptr;
    }
    FontBlob blob;
    blob.path = path;
    blob.data.resize((size_t)size);
    size_t read = fread(blob.data.data(), 1, (size_t)size, f);
    fclose(f);
    if (read != (size_t)size)
        return nullptr;
    g_fontBlobs.push_back(std::move(blob));
    return &g_fontBlobs.back().data;
}

// 加载一个字体 + 合并中文回退字体（共享文件缓存）。
ImFont *addFont(ImGuiIO &io, const char *latin, const char *cjk, float size)
{
    const std::vector<char> *latinBlob = fontBlob(fileExists(latin) ? latin : cjk);
    if (!latinBlob)
        return nullptr;

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = false;
    cfg.FontDataOwnedByAtlas = false; // 数据由 g_fontBlobs 持有
    ImFont *font = io.Fonts->AddFontFromMemoryTTF((void *)latinBlob->data(), (int)latinBlob->size(), size, &cfg);

    const std::vector<char> *cjkBlob = fontBlob(cjk);
    if (font && cjkBlob && cjkBlob != latinBlob)
    {
        ImFontConfig merge;
        merge.MergeMode = true;
        merge.OversampleH = 2;
        merge.OversampleV = 1;
        merge.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF((void *)cjkBlob->data(), (int)cjkBlob->size(), size, &merge);
    }
    return font;
}
} // namespace

void releaseFontData()
{
    g_fontBlobs.clear();
    g_fontBlobs.shrink_to_fit();
}

Fonts loadFonts()
{
    ImGuiIO &io = ImGui::GetIO();
    Fonts f;

    // Nunito → Segoe UI（同为人文主义无衬线，圆润度接近）；中文统一走微软雅黑
    const char *cjkReg = "C:/Windows/Fonts/msyh.ttc";
    const char *cjkBold = "C:/Windows/Fonts/msyhbd.ttc";

    f.body = addFont(io, "C:/Windows/Fonts/segoeui.ttf", cjkReg, 16.0f);
    f.bodyBold = addFont(io, "C:/Windows/Fonts/segoeuib.ttf", cjkBold, 16.0f);
    f.heading = addFont(io, "C:/Windows/Fonts/segoeuib.ttf", cjkBold, 18.0f);
    f.display = addFont(io, "C:/Windows/Fonts/segoeuib.ttf", cjkBold, 26.0f);
    f.caption = addFont(io, "C:/Windows/Fonts/segoeui.ttf", cjkReg, 13.0f);

    // JetBrains Mono → Cascadia Mono / Consolas
    const char *monoPath = "C:/Windows/Fonts/CascadiaMono.ttf";
    if (!fileExists(monoPath))
        monoPath = "C:/Windows/Fonts/consola.ttf";
    f.mono = addFont(io, monoPath, cjkReg, 15.0f);

    if (!f.body)
        f.body = io.Fonts->AddFontDefault();
    if (!f.bodyBold)
        f.bodyBold = f.body;
    if (!f.heading)
        f.heading = f.body;
    if (!f.display)
        f.display = f.heading;
    if (!f.caption)
        f.caption = f.body;
    if (!f.mono)
        f.mono = f.body;

    io.FontDefault = f.body;
    setFonts(f);
    return f;
}

void setFonts(const Fonts &f) { g_fonts = f; }
const Fonts &fonts() { return g_fonts; }

// ---------------------------------------------------------------------------
// 颜色
// ---------------------------------------------------------------------------
ImU32 withAlpha(ImU32 c, float a)
{
    if (a < 0.0f)
        a = 0.0f;
    if (a > 1.0f)
        a = 1.0f;
    ImU32 alpha = (ImU32)(a * 255.0f + 0.5f) << IM_COL32_A_SHIFT;
    return (c & ~IM_COL32_A_MASK) | alpha;
}

ImU32 mix(ImU32 a, ImU32 b, float t)
{
    if (t <= 0.0f)
        return a;
    if (t >= 1.0f)
        return b;
    int ar = (int)((a >> IM_COL32_R_SHIFT) & 0xFF);
    int ag = (int)((a >> IM_COL32_G_SHIFT) & 0xFF);
    int ab = (int)((a >> IM_COL32_B_SHIFT) & 0xFF);
    int aa = (int)((a >> IM_COL32_A_SHIFT) & 0xFF);
    int br = (int)((b >> IM_COL32_R_SHIFT) & 0xFF);
    int bg = (int)((b >> IM_COL32_G_SHIFT) & 0xFF);
    int bb = (int)((b >> IM_COL32_B_SHIFT) & 0xFF);
    int ba = (int)((b >> IM_COL32_A_SHIFT) & 0xFF);
    return IM_COL32((int)(ar + (br - ar) * t), (int)(ag + (bg - ag) * t),
                    (int)(ab + (bb - ab) * t), (int)(aa + (ba - aa) * t));
}

ImU32 lighten(ImU32 c, float f)
{
    int r = (int)((c >> IM_COL32_R_SHIFT) & 0xFF);
    int g = (int)((c >> IM_COL32_G_SHIFT) & 0xFF);
    int b = (int)((c >> IM_COL32_B_SHIFT) & 0xFF);
    r = (int)(r + (255 - r) * f);
    g = (int)(g + (255 - g) * f);
    b = (int)(b + (255 - b) * f);
    return IM_COL32(r, g, b, (c >> IM_COL32_A_SHIFT) & 0xFF);
}

// ---------------------------------------------------------------------------
// 动效
// ---------------------------------------------------------------------------
namespace
{
std::unordered_map<ImGuiID, float> g_anim;
int g_animating = 0;
} // namespace

float easeOut(float t)
{
    if (t <= 0.0f)
        return 0.0f;
    if (t >= 1.0f)
        return 1.0f;
    float u = 1.0f - t;
    return 1.0f - u * u * u; // cubic-bezier(.16,1,.3,1) 的近似
}

float easeSpring(float t)
{
    if (t <= 0.0f)
        return 0.0f;
    if (t >= 1.0f)
        return 1.0f;
    // cubic-bezier(.34,1.56,.64,1)：先冲过 1 再回落，用于「按下回弹」
    const float c = 1.70158f;
    float u = t - 1.0f;
    return 1.0f + (c + 1.0f) * u * u * u + c * u * u;
}

void beginFrame()
{
    g_animating = 0;
    // 动画条目不会太多（几十个控件），超过阈值时清理已收敛的旧条目
    if (g_anim.size() > 512)
    {
        for (auto it = g_anim.begin(); it != g_anim.end();)
        {
            if (it->second == 0.0f || it->second == 1.0f)
                it = g_anim.erase(it);
            else
                ++it;
        }
    }
}

bool animating() { return g_animating > 0; }

void markAnimating() { ++g_animating; }

float anim(ImGuiID id, float target, float speed)
{
    auto it = g_anim.find(id);
    if (it == g_anim.end())
    {
        g_anim.emplace(id, target);
        return target;
    }
    float &v = it->second;
    float dt = ImGui::GetIO().DeltaTime;
    if (dt > 0.05f)
        dt = 0.05f;
    if (dt <= 0.0f)
        dt = 1.0f / 60.0f;
    float k = 1.0f - expf(-speed * dt);
    v += (target - v) * k;
    if (fabsf(target - v) < 0.002f)
        v = target;
    else
        ++g_animating;
    return v;
}

// ---------------------------------------------------------------------------
// 全局样式
// ---------------------------------------------------------------------------
void applyStyle()
{
    ImGuiStyle &s = ImGui::GetStyle();
    ImGui::StyleColorsLight();

    // ---- 形状（Shape Lock）----
    s.WindowRounding = 0.0f;
    s.ChildRounding = T.radiusCard;
    s.FrameRounding = T.radiusInput;
    s.PopupRounding = T.radiusModal;
    s.ScrollbarRounding = 999.0f;
    s.GrabRounding = 999.0f;
    s.TabRounding = 999.0f;

    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 1.0f;
    s.FrameBorderSize = 1.0f;
    s.PopupBorderSize = 0.0f;
    s.TabBorderSize = 0.0f;

    // ---- 间距 ----
    s.WindowPadding = ImVec2(16, 16);
    s.FramePadding = ImVec2(12, 9);
    s.CellPadding = ImVec2(10, 8);
    s.ItemSpacing = ImVec2(8, 8);
    s.ItemInnerSpacing = ImVec2(6, 5);
    s.IndentSpacing = 18.0f;
    s.ScrollbarSize = 10.0f;
    s.GrabMinSize = 10.0f;

    s.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    s.WindowMenuButtonPosition = ImGuiDir_None;
    s.SeparatorTextBorderSize = 1.0f;

    s.AntiAliasedLines = true;
    s.AntiAliasedLinesUseTex = true;
    s.AntiAliasedFill = true;

    // ---- 颜色（对照 ac-tokens.css）----
    ImVec4 *c = s.Colors;
    auto set = [&](ImGuiCol idx, ImU32 col) { c[idx] = ImGui::ColorConvertU32ToFloat4(col); };

    set(ImGuiCol_Text, T.ink900);
    set(ImGuiCol_TextDisabled, T.ink500);
    set(ImGuiCol_WindowBg, T.bg);
    set(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    set(ImGuiCol_PopupBg, T.surface);
    set(ImGuiCol_Border, T.ink100);
    set(ImGuiCol_BorderShadow, IM_COL32(0, 0, 0, 0));

    set(ImGuiCol_FrameBg, T.surface2);
    set(ImGuiCol_FrameBgHovered, T.surface);
    set(ImGuiCol_FrameBgActive, T.pink50);
    set(ImGuiCol_TitleBg, T.bg);
    set(ImGuiCol_TitleBgActive, T.bg);
    set(ImGuiCol_TitleBgCollapsed, T.bg);
    set(ImGuiCol_MenuBarBg, T.surface);

    set(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
    set(ImGuiCol_ScrollbarGrab, T.ink300);
    set(ImGuiCol_ScrollbarGrabHovered, T.ink500);
    set(ImGuiCol_ScrollbarGrabActive, T.pink400);

    set(ImGuiCol_CheckMark, T.pink600);
    set(ImGuiCol_SliderGrab, T.pink500);
    set(ImGuiCol_SliderGrabActive, T.pink600);

    set(ImGuiCol_Button, T.pink100);
    set(ImGuiCol_ButtonHovered, T.pink200);
    set(ImGuiCol_ButtonActive, T.pink300);

    set(ImGuiCol_Header, T.pink50);
    set(ImGuiCol_HeaderHovered, T.surface2);
    set(ImGuiCol_HeaderActive, T.pink100);

    set(ImGuiCol_Separator, T.ink100);
    set(ImGuiCol_SeparatorHovered, T.pink300);
    set(ImGuiCol_SeparatorActive, T.pink500);
    set(ImGuiCol_ResizeGrip, T.pink200);
    set(ImGuiCol_ResizeGripHovered, T.pink300);
    set(ImGuiCol_ResizeGripActive, T.pink500);

    set(ImGuiCol_Tab, T.surface2);
    set(ImGuiCol_TabHovered, T.pink100);
    set(ImGuiCol_TabSelected, T.pink50);
    set(ImGuiCol_TabSelectedOverline, T.pink500);
    set(ImGuiCol_TabDimmed, T.surface2);
    set(ImGuiCol_TabDimmedSelected, T.pink50);
    set(ImGuiCol_TabDimmedSelectedOverline, T.pink200);

    set(ImGuiCol_PlotLines, T.pink500);
    set(ImGuiCol_PlotLinesHovered, T.pink600);
    set(ImGuiCol_PlotHistogram, T.pink500);
    set(ImGuiCol_PlotHistogramHovered, T.pink600);

    set(ImGuiCol_TableHeaderBg, T.surface2);
    set(ImGuiCol_TableBorderStrong, T.ink100);
    set(ImGuiCol_TableBorderLight, IM_COL32(245, 238, 242, 255));
    set(ImGuiCol_TableRowBg, IM_COL32(0, 0, 0, 0));
    set(ImGuiCol_TableRowBgAlt, T.pink50);

    set(ImGuiCol_TextSelectedBg, T.pink200);
    set(ImGuiCol_InputTextCursor, T.pink600);
    set(ImGuiCol_NavHighlight, T.pink200);
    set(ImGuiCol_DragDropTarget, T.pink500);
    set(ImGuiCol_ModalWindowDimBg, IM_COL32(50, 41, 49, 90)); // 禁纯黑蒙层

    // 输入框内边距（含边框 1px → 高度约 36px）
    s.FramePadding = ImVec2(12, 8);
}

} // namespace Theme
