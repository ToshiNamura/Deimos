#include "app.hpp"

#include <algorithm>
#include <string>

#include <deimos/support/strings.hpp>

#include "theme.hpp"
#include "widgets.hpp"

namespace deimos::ui
{
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
    if (seconds <= 0)
    {
        return "0s";
    }
    const std::int64_t hours = seconds / 3600;
    const std::int64_t minutes = (seconds % 3600) / 60;
    const std::int64_t remainder = seconds % 60;

    std::string text;
    if (hours > 0)
    {
        text += std::to_string(hours) + "h ";
    }
    if (hours > 0 || minutes > 0)
    {
        text += std::to_string(minutes) + "m ";
    }
    text += std::to_string(remainder) + "s";
    return text;
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
        heading("Daemon unreachable");
        spacer(4.0f);
        mutedText(current.error.empty() ? "The daemon did not answer the status request."
                                        : current.error.c_str());
        spacer(10.0f);
        mutedText("Start it with: deimosd --foreground, or add a remote daemon on the Daemons page.");
        spacer(10.0f);
        if (secondaryButton("Retry now"))
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
    sectionTitle("MODIFICATION ENGINE");
    {
        pushFont(fontHeading());
        ImGui::TextUnformatted(isRunning ? "Running" : "Stopped");
        popFont();
        ImGui::SameLine();
        pill(std::string{toString(engine.state)}.c_str(), stateColor(engine.state));
        if (!engine.activeProfileId.empty())
        {
            ImGui::SameLine();
            mutedText(engine.activeProfileId.c_str());
        }
    }
    spacer(6.0f);
    keyValue("Uptime", durationText(engine.uptimeSeconds));
    keyValue("Restarts", std::to_string(engine.restartCount));
    keyValue("Processes", std::to_string(engine.launches.size()));
    if (!engine.lastError.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(colors.danger));
        ImGui::TextWrapped("%s", engine.lastError.c_str());
        ImGui::PopStyleColor();
    }
    spacer(8.0f);

    if (isRunning)
    {
        if (secondaryButton("Stop")) 
        {
            stopEngine();
        }
        ImGui::SameLine();
        if (secondaryButton("Restart"))
        {
            restartEngine();
        }
    }
    else
    {
        if (primaryButton("Start"))
        {
            startEngine(engine.activeProfileId);
        }
        ImGui::SameLine();
        if (secondaryButton("Restart"))
        {
            restartEngine();
        }
    }
    cardEnd();

    // --- daemon card
    ImGui::SetCursorScreenPos(ImVec2(origin.x + leftWidth + 16.0f, origin.y));
    cardBegin("##daemon", ImVec2(rightWidth, CARD_HEIGHT));
    sectionTitle("DAEMON");
    keyValue("Version", current.info.version);
    keyValue("Platform", current.info.platform + "/" + current.info.architecture);
    keyValue("Host", current.info.hostname);
    keyValue("Uptime", durationText(current.info.uptimeSeconds));
    keyValue("Auth", current.info.authenticationRequired ? "bearer token" : "disabled");
    spacer(4.0f);
    if (current.info.zapretAvailable)
    {
        pill("zapret ready", colors.success);
        ImGui::SameLine();
        mutedText(strings::join(current.info.zapretTools, ", ").c_str());
    }
    else
    {
        pill("zapret missing", colors.danger);
        spacer(4.0f);
        mutedText("Set zapret.installDirectory in the daemon config.");
    }
    cardEnd();

    // --- activity card
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + CARD_HEIGHT + 16.0f));
    cardBegin("##activity", ImVec2(width, 0.0f));
    sectionTitle("RECENT ACTIVITY");
    const std::size_t limit = std::min<std::size_t>(current.logs.size(), 40);
    for (std::size_t index = current.logs.size(); index > current.logs.size() - limit; --index)
    {
        const LogEntry& entry = current.logs[index - 1];
        ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(colors.textMuted));
        ImGui::TextUnformatted(formatTimestamp(entry.timestamp).c_str() + 11);
        ImGui::PopStyleColor();
        ImGui::SameLine(110.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(levelColor(entry.level)));
        ImGui::Text("%-6s", std::string{toString(entry.level)}.c_str());
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
        mutedText("No log entries yet.");
    }
    cardEnd();
}

}
