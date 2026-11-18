#include "theme.hpp"

#include <filesystem>
#include <vector>

namespace deimos::ui
{
namespace
{

Palette g_palette;
ImFont* g_regular = nullptr;
ImFont* g_heading = nullptr;

ImU32 rgba(int red, int green, int blue, int alpha = 255)
{
    return IM_COL32(red, green, blue, alpha);
}

std::vector<std::string> fontCandidates()
{
#ifdef _WIN32
    return {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/tahoma.ttf", "C:/Windows/Fonts/arial.ttf"};
#elif defined(__APPLE__)
    return {"/System/Library/Fonts/SFNS.ttf", "/System/Library/Fonts/Helvetica.ttc"};
#else
    return {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf"};
#endif
}

} // namespace

const Palette& palette()
{
    return g_palette;
}

void applyTheme(float scale)
{
    g_palette.background = rgba(15, 17, 21);
    g_palette.backgroundDark = rgba(10, 12, 15);
    g_palette.panel = rgba(23, 27, 34);
    g_palette.panelAlt = rgba(19, 22, 28);
    g_palette.panelHover = rgba(31, 36, 45);
    g_palette.border = rgba(38, 44, 55);
    g_palette.text = rgba(230, 234, 242);
    g_palette.textMuted = rgba(138, 147, 165);
    g_palette.accent = rgba(76, 141, 255);
    g_palette.accentSoft = rgba(76, 141, 255, 46);
    g_palette.success = rgba(61, 220, 151);
    g_palette.warning = rgba(255, 176, 32);
    g_palette.danger = rgba(255, 92, 122);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.ChildRounding = 12.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(20.0f, 18.0f);
    style.FramePadding = ImVec2(12.0f, 8.0f);
    style.ItemSpacing = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 14.0f;
    style.IndentSpacing = 18.0f;
    style.ScaleAllSizes(scale);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImColor(g_palette.text);
    colors[ImGuiCol_TextDisabled] = ImColor(g_palette.textMuted);
    colors[ImGuiCol_WindowBg] = ImColor(g_palette.background);
    colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0, 0);
    colors[ImGuiCol_PopupBg] = ImColor(g_palette.panel);
    colors[ImGuiCol_Border] = ImColor(g_palette.border);
    colors[ImGuiCol_BorderShadow] = ImColor(0, 0, 0, 0);
    colors[ImGuiCol_FrameBg] = ImColor(g_palette.panelAlt);
    colors[ImGuiCol_FrameBgHovered] = ImColor(g_palette.panelHover);
    colors[ImGuiCol_FrameBgActive] = ImColor(g_palette.panelHover);
    colors[ImGuiCol_TitleBg] = ImColor(g_palette.backgroundDark);
    colors[ImGuiCol_TitleBgActive] = ImColor(g_palette.backgroundDark);
    colors[ImGuiCol_MenuBarBg] = ImColor(g_palette.backgroundDark);
    colors[ImGuiCol_ScrollbarBg] = ImColor(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrab] = ImColor(g_palette.border);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(g_palette.panelHover);
    colors[ImGuiCol_ScrollbarGrabActive] = ImColor(g_palette.panelHover);
    colors[ImGuiCol_CheckMark] = ImColor(g_palette.accent);
    colors[ImGuiCol_SliderGrab] = ImColor(g_palette.accent);
    colors[ImGuiCol_SliderGrabActive] = ImColor(g_palette.accent);
    colors[ImGuiCol_Button] = ImColor(g_palette.panelHover);
    colors[ImGuiCol_ButtonHovered] = ImColor(g_palette.border);
    colors[ImGuiCol_ButtonActive] = ImColor(g_palette.accentSoft);
    colors[ImGuiCol_Header] = ImColor(g_palette.panelHover);
    colors[ImGuiCol_HeaderHovered] = ImColor(g_palette.border);
    colors[ImGuiCol_HeaderActive] = ImColor(g_palette.accentSoft);
    colors[ImGuiCol_Separator] = ImColor(g_palette.border);
    colors[ImGuiCol_SeparatorHovered] = ImColor(g_palette.accent);
    colors[ImGuiCol_SeparatorActive] = ImColor(g_palette.accent);
    colors[ImGuiCol_ResizeGrip] = ImColor(g_palette.border);
    colors[ImGuiCol_ResizeGripHovered] = ImColor(g_palette.accent);
    colors[ImGuiCol_ResizeGripActive] = ImColor(g_palette.accent);
    colors[ImGuiCol_Tab] = ImColor(g_palette.panelAlt);
    colors[ImGuiCol_TabHovered] = ImColor(g_palette.panelHover);
    colors[ImGuiCol_TabSelected] = ImColor(g_palette.panelHover);
    colors[ImGuiCol_PlotLines] = ImColor(g_palette.accent);
    colors[ImGuiCol_PlotHistogram] = ImColor(g_palette.accent);
    colors[ImGuiCol_TableHeaderBg] = ImColor(g_palette.panelAlt);
    colors[ImGuiCol_TableBorderStrong] = ImColor(g_palette.border);
    colors[ImGuiCol_TableBorderLight] = ImColor(g_palette.border);
    colors[ImGuiCol_TableRowBg] = ImColor(0, 0, 0, 0);
    colors[ImGuiCol_TableRowBgAlt] = ImColor(255, 255, 255, 6);
    colors[ImGuiCol_TextSelectedBg] = ImColor(g_palette.accentSoft);
    colors[ImGuiCol_NavCursor] = ImColor(g_palette.accent);
    colors[ImGuiCol_ModalWindowDimBg] = ImColor(0, 0, 0, 160);
}

void loadFonts(float scale)
{
    ImGuiIO& io = ImGui::GetIO();
    const std::vector<std::string> candidates = fontCandidates();
    const float regularSize = 16.0f * scale;
    const float headingSize = 21.0f * scale;

    for (const std::string& path : candidates)
    {
        std::error_code code;
        if (!std::filesystem::is_regular_file(path, code))
        {
            continue;
        }
        g_regular = io.Fonts->AddFontFromFileTTF(path.c_str(), regularSize);
        g_heading = io.Fonts->AddFontFromFileTTF(path.c_str(), headingSize);
        if (g_regular != nullptr)
        {
            break;
        }
    }

    if (g_regular == nullptr)
    {
        g_regular = io.Fonts->AddFontDefault();
    }
    if (g_heading == nullptr)
    {
        g_heading = g_regular;
    }
}

ImFont* fontRegular()
{
    return g_regular;
}

ImFont* fontHeading()
{
    return g_heading;
}

void pushFont(ImFont* font)
{
    if (font == nullptr)
    {
        return;
    }
#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 19200
    ImGui::PushFont(font, 0.0f);
#else
    ImGui::PushFont(font);
#endif
}

void popFont()
{
    ImGui::PopFont();
}

}
