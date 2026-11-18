#include "app.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include <deimos/support/strings.hpp>

#include "theme.hpp"
#include "widgets.hpp"

namespace deimos::ui
{
namespace
{

ImU32 levelColor(LogLevel level)
{
    switch (level)
    {
    case LogLevel::trace:
    case LogLevel::debug:
        return palette().textMuted;
    case LogLevel::info:
        return palette().text;
    case LogLevel::warning:
        return palette().warning;
    case LogLevel::error:
        return palette().danger;
    }
    return palette().text;
}

bool matchesFilter(const LogEntry& entry, const std::string& filter)
{
    if (filter.empty())
    {
        return true;
    }
    const std::string needle = strings::toLower(filter);
    return strings::toLower(entry.message).find(needle) != std::string::npos ||
           strings::toLower(entry.source).find(needle) != std::string::npos;
}

} // namespace

void App::renderLogs()
{
    const UiSnapshot current = snapshot();
    const Palette& colors = palette();

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetWindowWidth() - 60.0f;

    ImGui::SetCursorScreenPos(origin);
    cardBegin("##logs", ImVec2(width, 0.0f));

    ImGui::PushItemWidth(240.0f);
    inputStringWithHint("##filter", "filter by text or source", logsFilter_);
    ImGui::PopItemWidth();
    ImGui::SameLine(300.0f);

    static int minimumLevel = 0;
    const char* levelLabels[] = {"trace", "debug", "info", "warn", "error"};
    ImGui::PushItemWidth(130.0f);
    ImGui::Combo("##level", &minimumLevel, levelLabels, IM_ARRAYSIZE(levelLabels));
    ImGui::PopItemWidth();
    ImGui::SameLine();

    toggleSwitch("Auto scroll", logsAutoScroll_);
    ImGui::SameLine();
    if (secondaryButton("Refresh"))
    {
        refreshNow();
    }
    ImGui::SameLine();
    mutedText((std::to_string(current.logs.size()) + " entries").c_str());

    spacer(6.0f);

    ImGui::BeginChild("##log-scroll", ImVec2(0.0f, 0.0f));
    const LogLevel threshold = static_cast<LogLevel>(minimumLevel);
    for (const LogEntry& entry : current.logs)
    {
        if (entry.level < threshold || !matchesFilter(entry, logsFilter_))
        {
            continue;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(colors.textMuted));
        ImGui::TextUnformatted(formatTimestamp(entry.timestamp).c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(190.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(levelColor(entry.level)));
        ImGui::Text("%-6s", std::string{toString(entry.level)}.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(250.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(colors.textMuted));
        ImGui::Text("[%s]", entry.source.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(380.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(levelColor(entry.level)));
        ImGui::TextWrapped("%s", entry.message.c_str());
        ImGui::PopStyleColor();
    }

    if (logsAutoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
    {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    cardEnd();
}

}
