#include "app.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <deimos/support/platform.hpp>
#include <deimos/support/strings.hpp>

#include "theme.hpp"
#include "widgets.hpp"

namespace deimos::ui
{

void App::renderDaemons()
{
    const UiSnapshot current = snapshot();
    const Palette& colors = palette();

    std::vector<EndpointRecord> endpoints;
    std::string currentName;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        endpoints = endpoints_;
        currentName = currentEndpoint_;
    }

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetWindowWidth() - 60.0f;
    const float listWidth = width * 0.55f;
    const float formWidth = width - listWidth - 16.0f;
    const float topHeight = 306.0f;

    // --- saved endpoints
    ImGui::SetCursorScreenPos(origin);
    cardBegin("##endpoints", ImVec2(listWidth, topHeight));
    sectionTitle("SAVED ENDPOINTS");
    ImGui::BeginChild("##endpoint-list", ImVec2(0.0f, topHeight - 70.0f));
    for (const EndpointRecord& record : endpoints)
    {
        ImGui::PushID(record.name.c_str());
        const bool selected = record.name == currentName;
        const ImVec2 rowStart = ImGui::GetCursorScreenPos();
        const float rowWidth = ImGui::GetContentRegionAvail().x;
        constexpr float ROW_HEIGHT = 52.0f;

        // Reserve the row first so the child boundaries stay consistent, then paint
        // the background and place the buttons on top of the reserved area.
        ImGui::Dummy(ImVec2(rowWidth, ROW_HEIGHT));
        const bool hovered = ImGui::IsItemHovered();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (selected)
        {
            drawList->AddRectFilled(rowStart, ImVec2(rowStart.x + rowWidth, rowStart.y + ROW_HEIGHT), colors.accentSoft, 10.0f);
        }
        else if (hovered)
        {
            drawList->AddRectFilled(rowStart, ImVec2(rowStart.x + rowWidth, rowStart.y + ROW_HEIGHT), colors.panelHover, 10.0f);
        }
        drawList->AddText(ImVec2(rowStart.x + 12.0f, rowStart.y + 9.0f), colors.text, record.name.c_str());
        drawList->AddText(ImVec2(rowStart.x + 12.0f, rowStart.y + 28.0f), colors.textMuted, record.url.c_str());

        ImGui::SetCursorScreenPos(ImVec2(rowStart.x + rowWidth - 170.0f, rowStart.y + 10.0f));
        if (!selected)
        {
            if (secondaryButton("Use", ImVec2(68.0f, 30.0f)))
            {
                selectEndpoint(record.name);
            }
            ImGui::SameLine();
        }
        if (dangerButton("Remove", ImVec2(88.0f, 30.0f)))
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                endpoints_.erase(std::remove_if(endpoints_.begin(),
                                                endpoints_.end(),
                                                [&record](const EndpointRecord& item)
                                                {
                                                    return item.name == record.name;
                                                }),
                                 endpoints_.end());
                if (currentEndpoint_ == record.name && !endpoints_.empty())
                {
                    currentEndpoint_ = endpoints_.front().name;
                }
            }
            persistState();
            refreshNow();
        }
        ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + ROW_HEIGHT));
        ImGui::PopID();
    }
    ImGui::EndChild();
    cardEnd();

    // --- add endpoint
    ImGui::SetCursorScreenPos(ImVec2(origin.x + listWidth + 16.0f, origin.y));
    cardBegin("##add-endpoint", ImVec2(formWidth, topHeight));
    sectionTitle("ADD DAEMON");
    ImGui::PushItemWidth(formWidth - 60.0f);
    ImGui::InputTextWithHint("##name", "name, for example router", newEndpointName_, IM_ARRAYSIZE(newEndpointName_));
    ImGui::InputTextWithHint("##url", "http://192.168.1.1:8787", newEndpointUrl_, IM_ARRAYSIZE(newEndpointUrl_));
    ImGui::InputTextWithHint("##token", "access token (optional)", newEndpointToken_, IM_ARRAYSIZE(newEndpointToken_),
                             ImGuiInputTextFlags_Password);
    ImGui::PopItemWidth();
    ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(colors.textMuted));
    ImGui::TextWrapped("Tokens are stored next to the Deimos configuration file.");
    ImGui::PopStyleColor();
    spacer(6.0f);
    if (primaryButton("Add daemon"))
    {
        Endpoint parsed;
        std::string errorMessage;
        const std::string url = newEndpointUrl_;
        if (newEndpointName_[0] == '\0')
        {
            toast("the daemon needs a name", true);
        }
        else if (!parseEndpoint(url, parsed, errorMessage))
        {
            toast("invalid url: " + errorMessage, true);
        }
        else
        {
            EndpointRecord record;
            record.name = newEndpointName_;
            record.url = parsed.describe();
            record.token = newEndpointToken_;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                endpoints_.erase(std::remove_if(endpoints_.begin(),
                                                endpoints_.end(),
                                                [&record](const EndpointRecord& item)
                                                {
                                                    return item.name == record.name;
                                                }),
                                 endpoints_.end());
                endpoints_.push_back(record);
            }
            newEndpointToken_[0] = '\0';
            persistState();
            refreshNow();
            toast("daemon '" + record.name + "' saved", false);
        }
    }
    cardEnd();

    // --- connection detail
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + topHeight + 16.0f));
    cardBegin("##connection", ImVec2(width, 0.0f));
    sectionTitle("CONNECTION");
    if (current.connected)
    {
        pill("connected", colors.success);
        ImGui::SameLine();
        mutedText(current.info.version.c_str());
        spacer(6.0f);
        keyValue("Endpoint", makeClient().endpoint().describe());
        keyValue("Host", current.info.hostname);
        keyValue("Platform", current.info.platform + "/" + current.info.architecture);
        keyValue("Daemon uptime", std::to_string(current.info.uptimeSeconds) + "s");
        keyValue("zapret", current.info.zapretAvailable ? strings::join(current.info.zapretTools, ", ")
                                                        : std::string{"not detected"});
    }
    else
    {
        pill("offline", colors.danger);
        spacer(6.0f);
        mutedText(current.error.empty() ? "No answer from the daemon." : current.error.c_str());
        spacer(6.0f);
        mutedText("Check that deimosd runs, that the port matches, and that the token is correct.");
    }
    spacer(10.0f);
    if (secondaryButton("Test connection"))
    {
        refreshNow();
    }
    ImGui::SameLine();
    if (secondaryButton("Run diagnostics"))
    {
        runDoctor();
    }
    cardEnd();
}

}
