#pragma once

#include <imgui.h>

namespace deimos::ui
{

struct Palette
{
    ImU32 background = 0;
    ImU32 backgroundDark = 0;
    ImU32 panel = 0;
    ImU32 panelAlt = 0;
    ImU32 panelHover = 0;
    ImU32 border = 0;
    ImU32 text = 0;
    ImU32 textMuted = 0;
    ImU32 accent = 0;
    ImU32 accentSoft = 0;
    ImU32 success = 0;
    ImU32 warning = 0;
    ImU32 danger = 0;
};

const Palette& palette();

/// ImGui has two PushStyleColor overloads; convert once to avoid ambiguity.
inline ImVec4 colorVec4(ImU32 color)
{
    return ImGui::ColorConvertU32ToFloat4(color);
}

/// Applies colours, rounding and spacing. \p scale is the display scale factor.
void applyTheme(float scale);

/// Loads the platform UI font at regular and heading sizes.
void loadFonts(float scale);

ImFont* fontRegular();
ImFont* fontHeading();

/// Pushes a font, tolerating both the 1.8x and 1.92+ ImGui signatures.
void pushFont(ImFont* font);
void popFont();

}
