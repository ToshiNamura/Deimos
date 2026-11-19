#include "app.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include <deimos/support/i18n.hpp>
#include <deimos/support/labels.hpp>
#include <deimos/support/strings.hpp>

#include "theme.hpp"
#include "widgets.hpp"

namespace deimos::ui
{
using i18n::tr;

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
    inputStringWithHint("##filter", tr("ui.logs.filterHint").c_str(), logsFilter_);
    ImGui::PopItemWidth();
    ImGui::SameLine(300.0f);

    static int minimumLevel = 0;
    const char* levelLabels[] = {labels::logLevel(LogLevel::trace).data(),
                                 labels::logLevel(LogLevel::debug).data(),
                                 labels::logLevel(LogLevel::info).data(),
                                 labels::logLevel(LogLevel::warning).data(),
                                 labels::logLevel(LogLevel::error).data()};
    ImGui::PushItemWidth(130.0f);
    ImGui::Combo("##level", &minimumLevel, levelLabels, IM_ARRAYSIZE(levelLabels));
    ImGui::PopItemWidth();
    ImGui::SameLine();

    toggleSwitch(tr("ui.logs.autoscroll").c_str(), logsAutoScroll_);
    ImGui::SameLine();
    if (secondaryButton(tr("ui.action.refresh").c_str()))
    {
        refreshNow();
    }
    ImGui::SameLine();
    mutedText(tr("ui.logs.entries", {std::to_string(current.logs.size())}).c_str());

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
        ImGui::Text("%-7s", std::string{labels::logLevel(entry.level)}.c_str());
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
