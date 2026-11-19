#include "app.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <deimos/support/i18n.hpp>
#include <deimos/support/labels.hpp>
#include <deimos/support/platform.hpp>
#include <deimos/support/strings.hpp>

#include "theme.hpp"
#include "widgets.hpp"

namespace deimos::ui
{
using i18n::tr;

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
    sectionTitle(tr("ui.daemons.saved").c_str());
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
            if (secondaryButton(tr("ui.action.use").c_str(), ImVec2(68.0f, 30.0f)))
            {
                selectEndpoint(record.name);
            }
            ImGui::SameLine();
        }
        if (dangerButton(tr("ui.action.remove").c_str(), ImVec2(88.0f, 30.0f)))
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
    sectionTitle(tr("ui.daemons.add").c_str());
    ImGui::PushItemWidth(formWidth - 60.0f);
    ImGui::InputTextWithHint("##name", tr("ui.daemons.nameHint").c_str(), newEndpointName_, IM_ARRAYSIZE(newEndpointName_));
    ImGui::InputTextWithHint("##url", "http://192.168.1.1:8787", newEndpointUrl_, IM_ARRAYSIZE(newEndpointUrl_));
    ImGui::InputTextWithHint("##token",
                             tr("ui.daemons.tokenHint").c_str(),
                             newEndpointToken_,
                             IM_ARRAYSIZE(newEndpointToken_),
                             ImGuiInputTextFlags_Password);
    ImGui::PopItemWidth();
    ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(colors.textMuted));
    ImGui::TextWrapped("%s", tr("ui.daemons.tokensHint").c_str());
    ImGui::PopStyleColor();
    spacer(6.0f);
    if (primaryButton(tr("ui.action.addDaemon").c_str()))
    {
        Endpoint parsed;
        std::string errorMessage;
        const std::string url = newEndpointUrl_;
        if (newEndpointName_[0] == '\0')
        {
            toast(tr("ui.toast.nameRequired"), true);
        }
        else if (!parseEndpoint(url, parsed, errorMessage))
        {
            toast(tr("ui.toast.invalidUrl", {errorMessage}), true);
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
            toast(tr("ui.toast.daemonSaved", {record.name}), false);
        }
    }
    cardEnd();

    // --- connection detail
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + topHeight + 16.0f));
    cardBegin("##connection", ImVec2(width, 0.0f));
    sectionTitle(tr("ui.daemons.connection").c_str());
    if (current.connected)
    {
        pill(tr("ui.status.connected").c_str(), colors.success);
        ImGui::SameLine();
        mutedText(current.info.version.c_str());
        spacer(6.0f);
        keyValue(tr("ui.field.endpoint").c_str(), makeClient().endpoint().describe());
        keyValue(tr("ui.field.host").c_str(), current.info.hostname);
        keyValue(tr("ui.field.platform").c_str(), current.info.platform + "/" + current.info.architecture);
        keyValue(tr("ui.field.daemonUptime").c_str(), labels::duration(current.info.uptimeSeconds));
        keyValue(tr("ui.field.zapret").c_str(),
                 current.info.zapretAvailable ? strings::join(current.info.zapretTools, ", ")
                                                        : std::string{tr("ui.value.notDetected")});
    }
    else
    {
        pill(tr("ui.status.offline").c_str(), colors.danger);
        spacer(6.0f);
        mutedText(current.error.empty() ? tr("ui.daemons.noAnswer").c_str() : current.error.c_str());
        spacer(6.0f);
        mutedText(tr("ui.daemons.offlineHint").c_str());
    }
    spacer(10.0f);
    if (secondaryButton(tr("ui.action.testConnection").c_str()))
    {
        refreshNow();
    }
    ImGui::SameLine();
    if (secondaryButton(tr("ui.action.runDiagnostics").c_str()))
    {
        runDoctor();
    }
    cardEnd();
}

}
