#include "widgets.hpp"

#include <cmath>
#include <cstring>

#include "theme.hpp"

namespace deimos::ui
{
namespace
{

ImU32 blend(ImU32 from, ImU32 to, float amount)
{
    const auto channel = [amount](ImU32 a, ImU32 b)
    {
        return static_cast<int>(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * amount);
    };
    const int red = channel((from >> IM_COL32_R_SHIFT) & 0xFF, (to >> IM_COL32_R_SHIFT) & 0xFF);
    const int green = channel((from >> IM_COL32_G_SHIFT) & 0xFF, (to >> IM_COL32_G_SHIFT) & 0xFF);
    const int blue = channel((from >> IM_COL32_B_SHIFT) & 0xFF, (to >> IM_COL32_B_SHIFT) & 0xFF);
    const int alpha = channel((from >> IM_COL32_A_SHIFT) & 0xFF, (to >> IM_COL32_A_SHIFT) & 0xFF);
    return IM_COL32(red, green, blue, alpha);
}

bool styledButton(const char* label, const ImVec2& size, ImU32 background, ImU32 textColor)
{
    ImVec2 actual = size;
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    if (actual.x <= 0.0f)
    {
        actual.x = textSize.x + 34.0f;
    }
    if (actual.y <= 0.0f)
    {
        actual.y = ImGui::GetFrameHeight() + 6.0f;
    }

    const ImVec2 position = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label, actual);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    ImU32 color = background;
    if (held)
    {
        color = blend(background, IM_COL32(0, 0, 0, 255), 0.18f);
    }
    else if (hovered)
    {
        color = blend(background, IM_COL32(255, 255, 255, 255), 0.12f);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(position, ImVec2(position.x + actual.x, position.y + actual.y), color, 8.0f);
    drawList->AddText(ImVec2(position.x + (actual.x - textSize.x) * 0.5f, position.y + (actual.y - textSize.y) * 0.5f),
                      textColor,
                      label);
    return ImGui::IsItemClicked();
}

int resizeCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        auto* value = static_cast<std::string*>(data->UserData);
        value->resize(static_cast<std::size_t>(data->BufTextLen));
        data->Buf = value->data();
    }
    return 0;
}

} // namespace

void drawIcon(ImDrawList* drawList, ImVec2 center, float size, Icon icon, ImU32 color)
{
    const float half = size * 0.5f;
    const float radius = 2.0f;
    switch (icon)
    {
    case Icon::dashboard:
    {
        const float cell = half * 0.78f;
        drawList->AddRectFilled(ImVec2(center.x - cell - 1.0f, center.y - cell - 1.0f),
                                ImVec2(center.x - 1.0f, center.y - 1.0f),
                                color,
                                radius);
        drawList->AddRectFilled(ImVec2(center.x + 1.0f, center.y - cell - 1.0f),
                                ImVec2(center.x + cell + 1.0f, center.y - 1.0f),
                                color,
                                radius);
        drawList->AddRectFilled(ImVec2(center.x - cell - 1.0f, center.y + 1.0f),
                                ImVec2(center.x - 1.0f, center.y + cell + 1.0f),
                                color,
                                radius);
        drawList->AddRectFilled(ImVec2(center.x + 1.0f, center.y + 1.0f),
                                ImVec2(center.x + cell + 1.0f, center.y + cell + 1.0f),
                                color,
                                radius);
        break;
    }
    case Icon::profiles:
    {
        for (int index = 0; index < 3; ++index)
        {
            const float offset = -half * 0.7f + static_cast<float>(index) * half * 0.7f;
            drawList->AddLine(ImVec2(center.x - half, center.y + offset),
                              ImVec2(center.x + half, center.y + offset),
                              color,
                              1.8f);
        }
        drawList->AddCircleFilled(ImVec2(center.x - half * 0.4f, center.y - half * 0.7f), 2.6f, color);
        drawList->AddCircleFilled(ImVec2(center.x + half * 0.5f, center.y), 2.6f, color);
        drawList->AddCircleFilled(ImVec2(center.x - half * 0.1f, center.y + half * 0.7f), 2.6f, color);
        break;
    }
    case Icon::server:
    {
        const float width = half * 1.8f;
        for (int index = 0; index < 2; ++index)
        {
            const float top = center.y - half + static_cast<float>(index) * (half * 1.05f);
            drawList->AddRect(ImVec2(center.x - width * 0.5f, top),
                              ImVec2(center.x + width * 0.5f, top + half * 0.75f),
                              color,
                              radius,
                              0,
                              1.6f);
            drawList->AddCircleFilled(ImVec2(center.x - width * 0.32f, top + half * 0.38f), 1.6f, color);
        }
        break;
    }
    case Icon::logs:
    {
        drawList->AddRect(ImVec2(center.x - half * 0.8f, center.y - half),
                          ImVec2(center.x + half * 0.8f, center.y + half),
                          color,
                          radius,
                          0,
                          1.6f);
        for (int index = 0; index < 3; ++index)
        {
            const float offset = -half * 0.45f + static_cast<float>(index) * half * 0.45f;
            drawList->AddLine(ImVec2(center.x - half * 0.4f, center.y + offset),
                              ImVec2(center.x + half * 0.4f, center.y + offset),
                              color,
                              1.4f);
        }
        break;
    }
    case Icon::play:
    {
        drawList->AddTriangleFilled(ImVec2(center.x - half * 0.5f, center.y - half * 0.75f),
                                    ImVec2(center.x - half * 0.5f, center.y + half * 0.75f),
                                    ImVec2(center.x + half * 0.8f, center.y),
                                    color);
        break;
    }
    case Icon::stop:
    {
        drawList->AddRectFilled(ImVec2(center.x - half * 0.62f, center.y - half * 0.62f),
                                ImVec2(center.x + half * 0.62f, center.y + half * 0.62f),
                                color,
                                2.0f);
        break;
    }
    case Icon::restart:
    {
        drawList->PathArcTo(center, half * 0.75f, 0.6f, 5.3f, 24);
        drawList->PathStroke(color, 0, 1.8f);
        drawList->AddTriangleFilled(ImVec2(center.x + half * 0.62f, center.y - half * 0.55f),
                                    ImVec2(center.x + half * 0.05f, center.y - half * 0.62f),
                                    ImVec2(center.x + half * 0.42f, center.y - half * 0.05f),
                                    color);
        break;
    }
    case Icon::check:
    {
        drawList->AddLine(ImVec2(center.x - half * 0.7f, center.y),
                          ImVec2(center.x - half * 0.1f, center.y + half * 0.55f),
                          color,
                          2.2f);
        drawList->AddLine(ImVec2(center.x - half * 0.1f, center.y + half * 0.55f),
                          ImVec2(center.x + half * 0.8f, center.y - half * 0.6f),
                          color,
                          2.2f);
        break;
    }
    case Icon::close:
    {
        drawList->AddLine(ImVec2(center.x - half * 0.55f, center.y - half * 0.55f),
                          ImVec2(center.x + half * 0.55f, center.y + half * 0.55f),
                          color,
                          2.0f);
        drawList->AddLine(ImVec2(center.x - half * 0.55f, center.y + half * 0.55f),
                          ImVec2(center.x + half * 0.55f, center.y - half * 0.55f),
                          color,
                          2.0f);
        break;
    }
    case Icon::bolt:
    {
        drawList->AddTriangleFilled(ImVec2(center.x, center.y - half),
                                    ImVec2(center.x + half * 0.7f, center.y - half * 0.05f),
                                    ImVec2(center.x + half * 0.1f, center.y + half * 0.05f),
                                    color);
        drawList->AddTriangleFilled(ImVec2(center.x, center.y + half),
                                    ImVec2(center.x - half * 0.7f, center.y + half * 0.05f),
                                    ImVec2(center.x - half * 0.1f, center.y - half * 0.05f),
                                    color);
        break;
    }
    case Icon::plus:
    {
        drawList->AddLine(ImVec2(center.x - half * 0.7f, center.y), ImVec2(center.x + half * 0.7f, center.y), color, 2.0f);
        drawList->AddLine(ImVec2(center.x, center.y - half * 0.7f), ImVec2(center.x, center.y + half * 0.7f), color, 2.0f);
        break;
    }
    case Icon::trash:
    {
        drawList->AddRect(ImVec2(center.x - half * 0.55f, center.y - half * 0.45f),
                          ImVec2(center.x + half * 0.55f, center.y + half * 0.8f),
                          color,
                          2.0f,
                          0,
                          1.6f);
        drawList->AddLine(ImVec2(center.x - half * 0.8f, center.y - half * 0.65f),
                          ImVec2(center.x + half * 0.8f, center.y - half * 0.65f),
                          color,
                          1.6f);
        drawList->AddLine(ImVec2(center.x - half * 0.2f, center.y - half * 0.65f),
                          ImVec2(center.x - half * 0.2f, center.y - half * 0.95f),
                          color,
                          1.6f);
        drawList->AddLine(ImVec2(center.x + half * 0.2f, center.y - half * 0.65f),
                          ImVec2(center.x + half * 0.2f, center.y - half * 0.95f),
                          color,
                          1.6f);
        drawList->AddLine(ImVec2(center.x - half * 0.2f, center.y - half * 0.95f),
                          ImVec2(center.x + half * 0.2f, center.y - half * 0.95f),
                          color,
                          1.6f);
        break;
    }
    case Icon::refresh:
    {
        drawList->PathArcTo(center, half * 0.75f, 0.8f, 5.6f, 24);
        drawList->PathStroke(color, 0, 1.8f);
        drawList->AddTriangleFilled(ImVec2(center.x + half * 0.75f, center.y + half * 0.3f),
                                    ImVec2(center.x + half * 0.15f, center.y + half * 0.55f),
                                    ImVec2(center.x + half * 0.6f, center.y + half * 0.85f),
                                    color);
        break;
    }
    }
}

bool navItem(const char* label, Icon icon, bool selected)
{
    const Palette& colors = palette();
    const float height = 42.0f;
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;

    ImGui::InvisibleButton(label, ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (selected)
    {
        drawList->AddRectFilled(position, ImVec2(position.x + width, position.y + height), colors.accentSoft, 10.0f);
        drawList->AddRectFilled(ImVec2(position.x, position.y + 10.0f),
                                ImVec2(position.x + 3.0f, position.y + height - 10.0f),
                                colors.accent,
                                2.0f);
    }
    else if (hovered)
    {
        drawList->AddRectFilled(position, ImVec2(position.x + width, position.y + height), colors.panelHover, 10.0f);
    }

    const ImU32 textColor = selected ? colors.text : (hovered ? colors.text : colors.textMuted);
    drawIcon(drawList, ImVec2(position.x + 24.0f, position.y + height * 0.5f), 16.0f, icon, selected ? colors.accent : textColor);
    drawList->AddText(ImVec2(position.x + 46.0f, position.y + (height - ImGui::GetTextLineHeight()) * 0.5f),
                      textColor,
                      label);
    return clicked;
}

bool primaryButton(const char* label, const ImVec2& size)
{
    return styledButton(label, size, palette().accent, IM_COL32(255, 255, 255, 255));
}

bool secondaryButton(const char* label, const ImVec2& size)
{
    return styledButton(label, size, palette().panelHover, palette().text);
}

bool dangerButton(const char* label, const ImVec2& size)
{
    return styledButton(label, size, IM_COL32(72, 32, 42, 255), palette().danger);
}

bool iconButton(const char* id, Icon icon, const char* tooltip)
{
    const float size = ImGui::GetFrameHeight();
    const ImVec2 position = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (hovered)
    {
        drawList->AddRectFilled(position, ImVec2(position.x + size, position.y + size), palette().panelHover, 8.0f);
    }
    drawIcon(drawList,
             ImVec2(position.x + size * 0.5f, position.y + size * 0.5f),
             14.0f,
             icon,
             hovered ? palette().text : palette().textMuted);
    if (tooltip != nullptr && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", tooltip);
    }
    return clicked;
}

void cardBegin(const char* id, const ImVec2& size)
{
    const Palette& colors = palette();
    ImVec2 actual = size;
    const ImVec2 available = ImGui::GetContentRegionAvail();
    if (actual.x <= 0.0f)
    {
        actual.x = available.x;
    }

    const ImVec2 position = ImGui::GetCursorScreenPos();
    const bool autoHeight = actual.y <= 0.0f;
    if (autoHeight)
    {
        actual.y = available.y;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(position, ImVec2(position.x + actual.x, position.y + actual.y), colors.panel, 14.0f);
    drawList->AddRect(position,
                      ImVec2(position.x + actual.x, position.y + actual.y),
                      colors.border,
                      14.0f,
                      0,
                      1.0f);

    ImGui::SetCursorScreenPos(ImVec2(position.x + 18.0f, position.y + 16.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::BeginChild(id,
                      ImVec2(actual.x - 36.0f, actual.y - 32.0f),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground);
}

void cardEnd()
{
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void pill(const char* text, ImU32 accentColor)
{
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 size(textSize.x + 22.0f, textSize.y + 8.0f);

    ImGui::InvisibleButton(text, size);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(position,
                            ImVec2(position.x + size.x, position.y + size.y),
                            blend(accentColor, palette().background, 0.78f),
                            9.0f);
    drawList->AddText(ImVec2(position.x + 11.0f, position.y + 4.0f), accentColor, text);
}

void heading(const char* text)
{
    pushFont(fontHeading());
    ImGui::TextUnformatted(text);
    popFont();
}

void mutedText(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(palette().textMuted));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void sectionTitle(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(palette().textMuted));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

void keyValue(const char* key, const std::string& value)
{
    ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(palette().textMuted));
    ImGui::TextUnformatted(key);
    ImGui::PopStyleColor();
    ImGui::SameLine(150.0f);
    ImGui::TextUnformatted(value.c_str());
}

void spacer(float height)
{
    ImGui::Dummy(ImVec2(0.0f, height));
}

bool toggleSwitch(const char* label, bool& value)
{
    const Palette& colors = palette();
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const float height = 22.0f;
    const float width = 42.0f;

    const bool hasLabel = label != nullptr && label[0] != '\0';
    const float labelWidth = hasLabel ? ImGui::CalcTextSize(label).x + 10.0f : 0.0f;

    ImGui::InvisibleButton(label != nullptr ? label : "##toggle", ImVec2(width + labelWidth, height));
    const bool clicked = ImGui::IsItemClicked();
    if (clicked)
    {
        value = !value;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 trackColor = value ? colors.accent : colors.border;
    drawList->AddRectFilled(position, ImVec2(position.x + width, position.y + height), trackColor, height * 0.5f);
    const float knobRadius = height * 0.5f - 3.0f;
    const float knobX = value ? position.x + width - knobRadius - 3.0f : position.x + knobRadius + 3.0f;
    drawList->AddCircleFilled(ImVec2(knobX, position.y + height * 0.5f), knobRadius, IM_COL32(255, 255, 255, 255));

    if (hasLabel)
    {
        drawList->AddText(ImVec2(position.x + width + 10.0f, position.y + (height - ImGui::GetTextLineHeight()) * 0.5f),
                          colors.text,
                          label);
    }
    return clicked;
}

bool inputString(const char* label, std::string& value, ImGuiInputTextFlags flags)
{
    if (value.capacity() < 32)
    {
        value.reserve(32);
    }
    return ImGui::InputText(label,
                            value.data(),
                            value.capacity() + 1,
                            flags | ImGuiInputTextFlags_CallbackResize,
                            resizeCallback,
                            &value);
}

bool inputStringWithHint(const char* label, const char* hint, std::string& value)
{
    if (value.capacity() < 32)
    {
        value.reserve(32);
    }
    return ImGui::InputTextWithHint(label,
                                    hint,
                                    value.data(),
                                    value.capacity() + 1,
                                    ImGuiInputTextFlags_CallbackResize,
                                    resizeCallback,
                                    &value);
}

bool multiLineString(const char* label, std::string& value, const ImVec2& size){
    if (value.capacity() < 256)
    {
        value.reserve(256);
    }
    return ImGui::InputTextMultiline(label,
                                     value.data(),
                                     value.capacity() + 1,
                                     size,
                                     ImGuiInputTextFlags_CallbackResize,
                                     resizeCallback,
                                     &value);
}

}
