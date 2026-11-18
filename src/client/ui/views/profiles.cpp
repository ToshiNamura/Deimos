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

ImU32 toolColor(ToolKind tool)
{
    switch (tool)
    {
    case ToolKind::winws:
        return palette().accent;
    case ToolKind::nfqws:
        return palette().success;
    case ToolKind::tpws:
        return palette().warning;
    case ToolKind::autoDetect:
        break;
    }
    return palette().textMuted;
}

bool optionalIntField(const char* label, std::optional<int>& value, int fallback)
{
    bool changed = false;
    const std::string identifier = std::string{"##"} + label + "enabled";
    bool enabled = value.has_value();
    if (ImGui::Checkbox(identifier.c_str(), &enabled))
    {
        if (enabled)
        {
            value = fallback;
        }
        else
        {
            value.reset();
        }
        changed = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!value.has_value());
    int buffer = value.value_or(fallback);
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::InputInt(label, &buffer, 0, 0))
    {
        if (value.has_value())
        {
            value = buffer;
            changed = true;
        }
    }
    ImGui::EndDisabled();
    return changed;
}

void checkboxTable(const char* id, const std::vector<const char*>& labels, std::vector<std::size_t>& selected)
{
    if (ImGui::BeginTable(id, 3, ImGuiTableFlags_SizingStretchSame))
    {
        for (std::size_t index = 0; index < labels.size(); ++index)
        {
            ImGui::TableNextColumn();
            const bool active = std::find(selected.begin(), selected.end(), index) != selected.end();
            bool toggled = active;
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::Checkbox(labels[index], &toggled))
            {
                if (toggled)
                {
                    selected.push_back(index);
                }
                else
                {
                    selected.erase(std::remove(selected.begin(), selected.end(), index), selected.end());
                }
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

template <typename Enum>
std::vector<std::size_t> selectedIndices(const std::vector<Enum>& values, const std::vector<Enum>& all)
{
    std::vector<std::size_t> indices;
    for (std::size_t index = 0; index < all.size(); ++index)
    {
        if (std::find(values.begin(), values.end(), all[index]) != values.end())
        {
            indices.push_back(index);
        }
    }
    return indices;
}

template <typename Enum>
std::vector<Enum> valuesFromIndices(const std::vector<std::size_t>& indices, const std::vector<Enum>& all)
{
    std::vector<Enum> values;
    for (const std::size_t index : indices)
    {
        if (index < all.size())
        {
            values.push_back(all[index]);
        }
    }
    return values;
}

std::vector<const char*> displayNames(const std::vector<DesyncMode>& values)
{
    std::vector<const char*> names;
    for (const DesyncMode value : values)
    {
        names.push_back(toDisplayName(value).data());
    }
    return names;
}

std::vector<const char*> displayNames(const std::vector<Fooling>& values)
{
    std::vector<const char*> names;
    for (const Fooling value : values)
    {
        names.push_back(toDisplayName(value).data());
    }
    return names;
}

std::vector<const char*> displayNames(const std::vector<L7Protocol>& values)
{
    std::vector<const char*> names;
    for (const L7Protocol value : values)
    {
        names.push_back(toDisplayName(value).data());
    }
    return names;
}

} // namespace

void App::renderProfiles()
{
    const UiSnapshot current = snapshot();
    const Palette& colors = palette();

    ProfileEditor editor;
    unsigned long long editorGeneration = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        editor = editor_;
        editorGeneration = editorGeneration_;
    }

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetWindowWidth() - 60.0f;
    const float listWidth = 285.0f;
    const float editorWidth = width - listWidth - 16.0f;
    const float cardHeight = ImGui::GetContentRegionAvail().y - 10.0f;

    // --- profile list
    ImGui::SetCursorScreenPos(origin);
    cardBegin("##profile-list", ImVec2(listWidth, cardHeight));

    if (primaryButton("New profile", ImVec2(listWidth - 40.0f, 0.0f)))
    {
        beginNewProfile();
        editor = ProfileEditor{};
        {
            std::lock_guard<std::mutex> lock(mutex_);
            editor = editor_;
        }
    }
    spacer(10.0f);

    ImGui::BeginChild("##profile-items", ImVec2(0.0f, 0.0f));
    if (current.profiles.empty())
    {
        mutedText("No profiles on this daemon yet.");
    }
    for (const ProfileSummary& summary : current.profiles)
    {
        const bool selected = editor.loaded && editor.originalId == summary.id;
        const ImVec2 rowStart = ImGui::GetCursorScreenPos();
        const float rowHeight = 58.0f;
        ImGui::PushID(summary.id.c_str());

        if (ImGui::InvisibleButton("##row", ImVec2(ImGui::GetContentRegionAvail().x, rowHeight)))
        {
            openProfile(summary.id);
        }
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (selected)
        {
            drawList->AddRectFilled(rowStart,
                                    ImVec2(rowStart.x + ImGui::GetContentRegionAvail().x, rowStart.y + rowHeight),
                                    colors.accentSoft,
                                    10.0f);
        }
        else if (hovered)
        {
            drawList->AddRectFilled(rowStart,
                                    ImVec2(rowStart.x + ImGui::GetContentRegionAvail().x, rowStart.y + rowHeight),
                                    colors.panelHover,
                                    10.0f);
        }
        drawList->AddText(ImVec2(rowStart.x + 12.0f, rowStart.y + 9.0f),
                          selected ? colors.text : colors.text,
                          summary.name.empty() ? summary.id.c_str() : summary.name.c_str());
        drawList->AddText(ImVec2(rowStart.x + 12.0f, rowStart.y + 30.0f), colors.textMuted, summary.id.c_str());

        const std::string toolText{toCliToken(summary.tool)};
        drawList->AddText(ImVec2(rowStart.x + ImGui::GetContentRegionAvail().x - 70.0f, rowStart.y + 20.0f),
                          toolColor(summary.tool),
                          toolText.c_str());
        if (!summary.enabled)
        {
            drawList->AddText(ImVec2(rowStart.x + ImGui::GetContentRegionAvail().x - 70.0f, rowStart.y + 36.0f),
                              colors.textMuted,
                              "disabled");
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    cardEnd();

    // --- editor
    ImGui::SetCursorScreenPos(ImVec2(origin.x + listWidth + 16.0f, origin.y));
    cardBegin("##profile-editor", ImVec2(editorWidth, cardHeight));

    if (!editor.loaded)
    {
        heading("Select a profile");
        spacer(6.0f);
        mutedText("Pick a profile on the left, or create a new one.");
        cardEnd();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            editor_ = editor;
        }
        return;
    }

    ImGui::BeginChild("##editor-scroll", ImVec2(0.0f, cardHeight - 90.0f));
    ImGui::PushItemWidth(320.0f);

    sectionTitle("IDENTITY");
    mutedText("ID");
    ImGui::BeginDisabled(!editor.isNew);
    inputString("##profile-id", editor.profile.id);
    ImGui::EndDisabled();
    mutedText("Name");
    inputString("##profile-name", editor.profile.name);
    mutedText("Description");
    ImGui::SetNextItemWidth(520.0f);
    inputString("##profile-description", editor.profile.description);

    spacer(6.0f);
    mutedText("Tool");
    ImGui::PushItemWidth(220.0f);
    int toolIndex = static_cast<int>(editor.profile.tool);
    const char* toolLabels[] = {"automatic", "winws (Windows)", "nfqws (Linux)", "tpws (Linux proxy)"};
    if (ImGui::Combo("##profile-tool", &toolIndex, toolLabels, IM_ARRAYSIZE(toolLabels)))
    {
        editor.profile.tool = static_cast<ToolKind>(toolIndex);
    }
    ImGui::PopItemWidth();
    mutedText("NFQUEUE number");
    ImGui::PushItemWidth(120.0f);
    int nfqueue = static_cast<int>(editor.profile.nfqueue);
    if (ImGui::InputInt("##profile-nfqueue", &nfqueue, 0, 0))
    {
        if (nfqueue > 0 && nfqueue <= 65535)
        {
            editor.profile.nfqueue = static_cast<std::uint16_t>(nfqueue);
        }
    }
    ImGui::PopItemWidth();
    ImGui::PopItemWidth();
    toggleSwitch("Enabled", editor.profile.enabled);

    spacer(8.0f);
    sectionTitle("ROUNDS");
    for (std::size_t index = 0; index < editor.profile.rounds.size(); ++index)
    {
        DesyncRound& round = editor.profile.rounds[index];
        if (index >= editor.rounds.size())
        {
            editor.rounds.resize(index + 1);
        }
        RoundEditor& roundEditor = editor.rounds[index];

        ImGui::PushID(static_cast<int>(index));
        const std::string header = "Round " + std::to_string(index + 1);
        const bool open = ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        if (editor.profile.rounds.size() > 1)
        {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
            if (iconButton("##remove-round", Icon::trash, "Remove this round"))
            {
                editor.profile.rounds.erase(editor.profile.rounds.begin() + static_cast<std::ptrdiff_t>(index));
                editor.rounds.erase(editor.rounds.begin() + static_cast<std::ptrdiff_t>(index));
                ImGui::PopID();
                break;
            }
        }
        if (open)
        {
            mutedText("TCP ports");
            ImGui::PushItemWidth(220.0f);
            inputString("##tcp-ports", roundEditor.tcpText);
            ImGui::PopItemWidth();
            mutedText("UDP ports");
            ImGui::PushItemWidth(220.0f);
            inputString("##udp-ports", roundEditor.udpText);
            ImGui::PopItemWidth();
            mutedText("Comma separated, for example 80,443");

            auto l7Indices = selectedIndices(round.filter.l7, allL7Protocols());
            checkboxTable("##l7", displayNames(allL7Protocols()), l7Indices);
            round.filter.l7 = valuesFromIndices(l7Indices, allL7Protocols());

            auto modeIndices = selectedIndices(round.modes, allDesyncModes());
            checkboxTable("##modes", displayNames(allDesyncModes()), modeIndices);
            round.modes = valuesFromIndices(modeIndices, allDesyncModes());

            auto foolingIndices = selectedIndices(round.fooling, allFoolingFlags());
            checkboxTable("##fooling", displayNames(allFoolingFlags()), foolingIndices);
            round.fooling = valuesFromIndices(foolingIndices, allFoolingFlags());

            spacer(4.0f);
            optionalIntField("Split position", round.splitPos, 1);
            optionalIntField("Automatic TTL", round.autoTtl, 2);
            optionalIntField("TTL", round.ttl, 5);
            optionalIntField("Repeats", round.repeats, 6);

            bool hasPayload = round.fakePayload.has_value();
            if (toggleSwitch("Fake payload", hasPayload))
            {
                if (hasPayload)
                {
                    round.fakePayload = FakePayload{};
                }
                else
                {
                    round.fakePayload.reset();
                }
            }
            if (round.fakePayload.has_value())
            {
                ImGui::PushItemWidth(140.0f);
                int kindIndex = round.fakePayload->kind == FakePayloadKind::hex ? 1 : 0;
                const char* kindLabels[] = {"file", "inline hex"};
                if (ImGui::Combo("##payload-kind", &kindIndex, kindLabels, IM_ARRAYSIZE(kindLabels)))
                {
                    round.fakePayload->kind = kindIndex == 1 ? FakePayloadKind::hex : FakePayloadKind::file;
                }
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::PushItemWidth(320.0f);
                inputString("##payload-value", round.fakePayload->value);
                ImGui::PopItemWidth();
            }

            spacer(6.0f);
            ImGui::PushItemWidth(420.0f);
            ImGui::TextUnformatted("Hostlists");
            multiLineString("##hostlists-round", roundEditor.hostlists, ImVec2(420.0f, 46.0f));
            ImGui::PopItemWidth();
        }
        ImGui::PopID();
    }
    ImGui::Spacing();
    if (secondaryButton("Add round"))
    {
        editor.profile.rounds.push_back(DesyncRound{});
        editor.rounds.push_back(RoundEditor{});
    }

    spacer(10.0f);
    sectionTitle("SHARED LISTS");
    ImGui::TextUnformatted("Hostlists");
    multiLineString("##hostlists", editor.hostlists, ImVec2(420.0f, 56.0f));
    ImGui::TextUnformatted("Excluded hostlists");
    multiLineString("##hostlist-excludes", editor.hostlistExcludes, ImVec2(420.0f, 46.0f));
    ImGui::TextUnformatted("IP sets");
    multiLineString("##ipsets", editor.ipsets, ImVec2(420.0f, 46.0f));
    ImGui::TextUnformatted("Extra zapret arguments");
    multiLineString("##extra-args", editor.extraArguments, ImVec2(420.0f, 56.0f));

    if (!editor.error.empty())
    {
        spacer(6.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(colors.danger));
        ImGui::TextWrapped("%s", editor.error.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();

    spacer(6.0f);
    if (primaryButton("Save"))
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            editor_ = editor;
        }
        saveEditorProfile();
    }
    ImGui::SameLine();
    if (secondaryButton("Preview command"))
    {
        if (editor.isNew)
        {
            toast("save the profile before previewing the command line", true);
        }
        else
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                editor_ = editor;
            }
            requestPreview(editor.originalId.empty() ? editor.profile.id : editor.originalId);
        }
    }
    ImGui::SameLine();
    if (!editor.isNew && dangerButton("Delete"))
    {
        deleteProfile(editor.originalId);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Skip the copy-back when a background action loaded or replaced the editor
        // while this frame was being built; its data is newer than ours.
        if (editorGeneration_ == editorGeneration)
        {
            if (editor.error.empty() && !editor_.error.empty())
            {
                editor.error = editor_.error;
            }
            if (editor.originalId.empty() && !editor_.originalId.empty())
            {
                editor.originalId = editor_.originalId;
                editor.isNew = false;
            }
            editor_ = editor;
        }
    }
    cardEnd();

    // --- preview popup
    std::vector<std::string> commands;
    std::string diagnostics;
    bool showPreview = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        commands = previewCommands_;
        diagnostics = previewDiagnostics_;
        showPreview = showPreview_;
    }
    if (showPreview)
    {
        ImGui::OpenPopup("Command preview");
        ImGui::SetNextWindowSize(ImVec2(760.0f, 0.0f));
        if (ImGui::BeginPopupModal("Command preview", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            mutedText("Deimos runs these commands on the daemon host:");
            spacer(6.0f);
            for (const std::string& command : commands)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(colors.text));
                ImGui::TextWrapped("%s", command.c_str());
                ImGui::PopStyleColor();
                spacer(4.0f);
            }
            if (commands.empty())
            {
                mutedText("No commands were produced.");
            }
            if (!diagnostics.empty())
            {
                spacer(6.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, colorVec4(colors.warning));
                ImGui::TextWrapped("%s", diagnostics.c_str());
                ImGui::PopStyleColor();
            }
            spacer(8.0f);
            if (primaryButton("Close"))
            {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    showPreview_ = false;
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

}
