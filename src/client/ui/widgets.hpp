#pragma once

#include <string>

#include <imgui.h>

namespace deimos::ui
{

enum class Icon
{
    dashboard,
    profiles,
    server,
    logs,
    play,
    stop,
    restart,
    check,
    close,
    bolt,
    plus,
    trash,
    refresh,
};

void drawIcon(ImDrawList* drawList, ImVec2 center, float size, Icon icon, ImU32 color);

bool navItem(const char* label, Icon icon, bool selected);

bool primaryButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f));
bool secondaryButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f));
bool dangerButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f));
bool iconButton(const char* id, Icon icon, const char* tooltip = nullptr);

void cardBegin(const char* id, const ImVec2& size = ImVec2(0.0f, 0.0f));
void cardEnd();

void pill(const char* text, ImU32 accentColor);
void heading(const char* text);
void mutedText(const char* text);
void sectionTitle(const char* text);
void keyValue(const char* key, const std::string& value);
void spacer(float height);

bool toggleSwitch(const char* label, bool& value);
bool inputString(const char* label, std::string& value, ImGuiInputTextFlags flags = 0);
bool inputStringWithHint(const char* label, const char* hint, std::string& value);
bool multiLineString(const char* label, std::string& value, const ImVec2& size);

}
