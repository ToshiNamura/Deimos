#include "app.hpp"

#include <algorithm>
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

ImU32 stateColor(EngineState state)
{
    switch (state)
    {
    case EngineState::running:
        return palette().success;
    case EngineState::starting:
    case EngineState::stopping:
        return palette().warning;
    case EngineState::faulted:
        return palette().danger;
    case EngineState::stopped:
        break;
    }
    return palette().textMuted;
}

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

std::string durationText(std::int64_t seconds)
{
    return labels::duration(seconds);
}

} // namespace

void App::renderDashboard()
{
    const UiSnapshot current = snapshot();
    const Palette& colors = palette();

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetWindowWidth() - 60.0f;
    const float leftWidth = width * 0.59f;
    const float rightWidth = width - leftWidth - 16.0f;
    constexpr float CARD_HEIGHT = 292.0f;

    if (!current.connected)
    {
        ImGui::SetCursorScreenPos(origin);
        cardBegin("##unreachable", ImVec2(width, 170.0f));
        heading(tr("ui.dash.unreachableTitle").c_str());
        spacer(4.0f);
        mutedText(current.error.empty() ? tr("ui.dash.unreachableReason").c_str() : current.error.c_str());
        spacer(10.0f);
        mutedText(tr("ui.dash.unreachableHint").c_str());
        spacer(10.0f);
        if (secondaryButton(tr("ui.action.retry").c_str()))
        {
            refreshNow();
        }
        cardEnd();
        return;
    }

    const EngineSnapshot& engine = current.info.engine;
    const bool isRunning = engine.state == EngineState::running;

    // --- engine card
    ImGui::SetCursorScreenPos(origin);
    cardBegin("##engine", ImVec2(leftWidth, CARD_HEIGHT));
    sectionTitle(tr("ui.dash.engine").c_str());
    {
        pushFont(fontHeading());
        ImGui::TextUnformatted(tr(isRunning ? "state.running" : "state.stopped").c_str());
        popFont();
        ImGui::SameLine();
        pill(std::string{labels::engineState(engine.state)}.c_str(), stateColor(engine.state));
        if (!engine.activeProfileId.empty())
        {
            ImGui::SameLine();
            mutedText(engine.activeProfileId.c_str());
        }
    }
    spacer(6.0f);
    keyValue(tr("ui.field.uptime").c_str(), durationText(engine.uptimeSeconds));
    keyValue(tr("ui.field.restarts").c_str(), std::to_string(engine.restartCount));
    keyValue(tr("ui.field.processes").c_str(), std::to_string(engine.launches.size()));
    if (!engine.lastError.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(colors.danger));
        ImGui::TextWrapped("%s", engine.lastError.c_str());
        ImGui::PopStyleColor();
    }
    spacer(8.0f);

    if (isRunning)
    {
        if (secondaryButton(tr("ui.action.stop").c_str())) 
        {
            stopEngine();
        }
        ImGui::SameLine();
        if (secondaryButton(tr("ui.action.restart").c_str()))
        {
            restartEngine();
        }
    }
    else
    {
        if (primaryButton(tr("ui.action.start").c_str()))
        {
            startEngine(engine.activeProfileId);
        }
        ImGui::SameLine();
        if (secondaryButton(tr("ui.action.restart").c_str()))
        {
            restartEngine();
        }
    }
    cardEnd();

    // --- daemon card
    ImGui::SetCursorScreenPos(ImVec2(origin.x + leftWidth + 16.0f, origin.y));
    cardBegin("##daemon", ImVec2(rightWidth, CARD_HEIGHT));
    sectionTitle(tr("ui.dash.daemon").c_str());
    keyValue(tr("ui.field.version").c_str(), current.info.version);
    keyValue(tr("ui.field.platform").c_str(), current.info.platform + "/" + current.info.architecture);
    keyValue(tr("ui.field.host").c_str(), current.info.hostname);
    keyValue(tr("ui.field.uptime").c_str(), durationText(current.info.uptimeSeconds));
    keyValue(tr("ui.field.auth").c_str(),
             tr(current.info.authenticationRequired ? "ui.value.bearerToken" : "ui.value.disabled"));
    spacer(4.0f);
    if (current.info.zapretAvailable)
    {
        pill(tr("ui.value.zapretReady").c_str(), colors.success);
        ImGui::SameLine();
        mutedText(strings::join(current.info.zapretTools, ", ").c_str());
    }
    else
    {
        pill(tr("ui.value.zapretMissing").c_str(), colors.danger);
        spacer(4.0f);
        mutedText(tr("ui.dash.zapretHint").c_str());
    }
    cardEnd();

    // --- activity card
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + CARD_HEIGHT + 16.0f));
    cardBegin("##activity", ImVec2(width, 0.0f));
    sectionTitle(tr("ui.dash.activity").c_str());
    const std::size_t limit = std::min<std::size_t>(current.logs.size(), 40);
    for (std::size_t index = current.logs.size(); index > current.logs.size() - limit; --index)
    {
        const LogEntry& entry = current.logs[index - 1];
        ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(colors.textMuted));
        ImGui::TextUnformatted(formatTimestamp(entry.timestamp).c_str() + 11);
        ImGui::PopStyleColor();
        ImGui::SameLine(110.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(levelColor(entry.level)));
        ImGui::Text("%-7s", std::string{labels::logLevel(entry.level)}.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(170.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(colors.textMuted));
        ImGui::Text("[%s]", entry.source.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextUnformatted(entry.message.c_str());
    }
    if (limit == 0)
    {
        mutedText(tr("ui.logs.empty").c_str());
    }
    cardEnd();
}

}
