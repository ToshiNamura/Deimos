#include "app.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <utility>

#include <nlohmann/json.hpp>

#include <deimos/support/i18n.hpp>
#include <deimos/support/labels.hpp>
#include <deimos/support/log.hpp>
#include <deimos/support/platform.hpp>
#include <deimos/support/strings.hpp>
#include <deimos/version.hpp>

#include "theme.hpp"
#include "widgets.hpp"

namespace deimos::ui
{
using i18n::tr;

namespace
{

constexpr float SIDEBAR_WIDTH = 236.0f;
constexpr const char* APP_NAME = "Deimos";

std::filesystem::path statePath()
{
    return platform::configDirectory() / "ui.json";
}

std::string readTokenFile(const std::string& path)
{
    if (path.empty())
    {
        return {};
    }
    const auto content = platform::readFile(std::filesystem::path{path});
    return content.has_value() ? strings::trim(*content) : std::string{};
}

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

const std::string& titleFor(int view)
{
    switch (static_cast<int>(view))
    {
    case 0:
        return tr("ui.title.dashboard");
    case 1:
        return tr("ui.title.profiles");
    case 2:
        return tr("ui.title.daemons");
    default:
        return tr("ui.title.logs");
    }
}

std::vector<std::string> splitLines(const std::string& text)
{
    std::vector<std::string> lines;
    for (const std::string& line : strings::split(text, '\n'))
    {
        const std::string trimmed = strings::trim(line);
        if (!trimmed.empty())
        {
            lines.push_back(trimmed);
        }
    }
    return lines;
}

std::string joinLines(const std::vector<std::string>& lines)
{
    return strings::join(lines, "\n");
}

} // namespace

App::App() = default;

App::~App()
{
    shutdown();
}

bool App::initialize()
{
    loadState();

    running_.store(true);

    workerRunning_ = true;
    worker_ = std::thread(
        [this]
        {
            for (;;)
            {
                std::pair<std::string, std::function<void()>> item;
                {
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    queueCv_.wait(lock,
                                  [this]
                                  {
                                      return !queue_.empty() || !workerRunning_;
                                  });
                    if (!workerRunning_ && queue_.empty())
                    {
                        return;
                    }
                    item = std::move(queue_.front());
                    queue_.pop_front();
                }

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    busyLabel_ = item.first;
                }

                try
                {
                    item.second();
                }
                catch (const std::exception& exception)
                {
                    toast(std::string{"unexpected error: "} + exception.what(), true);
                }

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    pendingActions_ = std::max(0, pendingActions_ - 1);
                    if (pendingActions_ == 0)
                    {
                        busyLabel_.clear();
                    }
                }
                refreshRequested_.store(true);
            }
        });

    pollThread_ = std::thread(&App::pollLoop, this);
    refreshRequested_.store(true);
    return true;
}

void App::shutdown()
{
    running_.store(false);
    if (pollThread_.joinable())
    {
        pollThread_.join();
    }
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        workerRunning_ = false;
    }
    queueCv_.notify_all();
    if (worker_.joinable())
    {
        worker_.join();
    }
}

// ------------------------------------------------------------------ persisted state

void App::loadState()
{
    std::vector<EndpointRecord> endpoints;
    std::string current;
    std::string language{"auto"};

    const auto content = platform::readFile(statePath());
    if (content.has_value())
    {
        try
        {
            const auto root = nlohmann::json::parse(*content);
            current = root.value("current", std::string{});
            language = root.value("language", std::string{"auto"});
            refreshSeconds_.store(root.value("refreshSeconds", 2));
            const auto list = root.find("endpoints");
            if (list != root.end() && list->is_array())
            {
                for (const auto& item : *list)
                {
                    EndpointRecord record;
                    record.name = item.value("name", std::string{});
                    record.url = item.value("url", std::string{"http://127.0.0.1:8787"});
                    record.tokenFile = item.value("tokenFile", std::string{});
                    record.token = item.value("token", std::string{});
                    if (!record.name.empty())
                    {
                        endpoints.push_back(std::move(record));
                    }
                }
            }
        }
        catch (const std::exception&)
        {
            endpoints.clear();
        }
    }

    if (endpoints.empty())
    {
        EndpointRecord local;
        local.name = "local";
        local.url = "http://127.0.0.1:8787";
        endpoints.push_back(std::move(local));
        current = "local";
    }
    if (current.empty() || std::none_of(endpoints.begin(), endpoints.end(), [&current](const EndpointRecord& record)
        {
            return record.name == current;
        }))
    {
        current = endpoints.front().name;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    endpoints_ = std::move(endpoints);
    currentEndpoint_ = std::move(current);
    languageSetting_ = languageOverride_.empty() ? std::move(language) : languageOverride_;
    i18n::setLanguageByCode(languageSetting_);
    newEndpoint_.name = "remote";
    newEndpoint_.url = "http://192.168.1.1:8787";

    std::snprintf(newEndpointName_, sizeof(newEndpointName_), "%s", "remote");
    std::snprintf(newEndpointUrl_, sizeof(newEndpointUrl_), "%s", "http://192.168.1.1:8787");
    newEndpointToken_[0] = '\0';
}

void App::persistState()
{
    std::vector<EndpointRecord> endpoints;
    std::string current;
    std::string language;
    int refreshSeconds = 2;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        endpoints = endpoints_;
        current = currentEndpoint_;
        language = languageSetting_;
        refreshSeconds = refreshSeconds_.load();
    }

    nlohmann::json list = nlohmann::json::array();
    for (const EndpointRecord& record : endpoints)
    {
        nlohmann::json item;
        item["name"] = record.name;
        item["url"] = record.url;
        if (!record.tokenFile.empty())
        {
            item["tokenFile"] = record.tokenFile;
        }
        if (!record.token.empty())
        {
            item["token"] = record.token;
        }
        list.push_back(std::move(item));
    }

    nlohmann::json root;
    root["current"] = current;
    root["language"] = language;
    root["refreshSeconds"] = refreshSeconds;
    root["endpoints"] = std::move(list);

    platform::ensureDirectory(statePath().parent_path());
    std::string errorMessage;
    if (!platform::writeFileAtomically(statePath(), root.dump(2) + "\n", errorMessage))
    {
        log::warning("ui", i18n::tr("log.ui.saveFailed", {statePath().string(), errorMessage}));
    }
    platform::restrictFilePermissions(statePath());
}

// ------------------------------------------------------------------ helpers

UiSnapshot App::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

EndpointRecord App::activeEndpoint() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const EndpointRecord& record : endpoints_)
    {
        if (record.name == currentEndpoint_)
        {
            return record;
        }
    }
    if (!endpoints_.empty())
    {
        return endpoints_.front();
    }
    EndpointRecord fallback;
    fallback.name = "local";
    return fallback;
}

void App::selectEndpoint(const std::string& name)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentEndpoint_ = name;
    }
    persistState();
    refreshRequested_.store(true);
}

void App::setLanguageOverride(std::string code)
{
    languageOverride_ = std::move(code);
}

void App::setLanguageSetting(const std::string& code)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        languageSetting_ = code;
    }
    i18n::setLanguageByCode(code);
    persistState();
}

DaemonClient App::makeClient() const
{
    const EndpointRecord record = activeEndpoint();

    Endpoint parsed;
    std::string errorMessage;
    if (!parseEndpoint(record.url, parsed, errorMessage))
    {
        parsed = Endpoint{};
    }

    std::string token = record.token;
    if (token.empty())
    {
        token = readTokenFile(record.tokenFile);
    }
    if (token.empty())
    {
        token = readTokenFile(defaultTokenPath());
    }

    ClientOptions options;
    options.endpoint = parsed;
    options.token = std::move(token);
    options.timeoutSeconds = 8;
    return DaemonClient{std::move(options)};
}

// ------------------------------------------------------------------ polling

void App::pollLoop()
{
    int ticks = 0;
    while (running_.load())
    {
        const bool requested = refreshRequested_.exchange(false);
        if (requested || ticks <= 0)
        {
            pollOnce();
            ticks = std::max(1, refreshSeconds_.load()) * 20;
        }
        for (int tick = 0; tick < 20 && running_.load(); ++tick)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{50});
        }
        --ticks;
    }
}

void App::pollOnce()
{
    DaemonClient client = makeClient();

    UiSnapshot fresh;
    fresh.fetchedAt = std::chrono::system_clock::now();

    const auto status = client.status();
    if (!status)
    {
        fresh.connected = false;
        fresh.error = status.error().describe();
    }
    else
    {
        fresh.connected = true;
        fresh.info = status.value();
        if (const auto profiles = client.listProfiles())
        {
            fresh.profiles = profiles.value();
        }
        if (const auto entries = client.logs(300, LogLevel::trace))
        {
            fresh.logs = entries.value();
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_ = std::move(fresh);
}

void App::refreshNow()
{
    refreshRequested_.store(true);
}

// ------------------------------------------------------------------ actions

void App::runAsync(const std::string& label, std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++pendingActions_;
        busyLabel_ = label;
    }
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.emplace_back(label, std::move(task));
    }
    queueCv_.notify_one();
}

void App::toast(std::string text, bool isError)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Toast toast;
    toast.text = std::move(text);
    toast.isError = isError;
    toasts_.push_front(std::move(toast));
    while (toasts_.size() > 3)
    {
        toasts_.pop_back();
    }
}

bool App::isBusy() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return pendingActions_ > 0;
}

std::string App::busyLabel() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return busyLabel_;
}

void App::startEngine(const std::string& profileId)
{
    runAsync(tr("ui.busy.starting"),
             [this, profileId]
             {
                 DaemonClient client = makeClient();
                 const auto result = client.startEngine(profileId);
                 if (!result)
                 {
                     toast(tr("ui.toast.startFailed", {result.error().describe()}), true);
                     return;
                 }
                 toast(tr("ui.toast.engineState", {std::string{labels::engineState(result.value().state)}}), false);
             });
}

void App::stopEngine()
{
    runAsync(tr("ui.busy.stopping"),
             [this]
             {
                 DaemonClient client = makeClient();
                 const auto result = client.stopEngine();
                 if (!result)
                 {
                     toast(tr("ui.toast.stopFailed", {result.error().describe()}), true);
                     return;
                 }
                 toast(tr("ui.toast.stopped"), false);
             });
}

void App::restartEngine()
{
    runAsync(tr("ui.busy.restarting"),
             [this]
             {
                 DaemonClient client = makeClient();
                 const auto result = client.restartEngine();
                 if (!result)
                 {
                     toast(tr("ui.toast.restartFailed", {result.error().describe()}), true);
                     return;
                 }
                 toast(tr("ui.toast.restarted"), false);
             });
}

void App::activateProfile(const std::string& profileId)
{
    runAsync(tr("ui.busy.activating"),
             [this, profileId]
             {
                 DaemonClient client = makeClient();
                 const auto result = client.activateProfile(profileId);
                 if (!result)
                 {
                     toast(tr("ui.toast.activateFailed", {result.error().describe()}), true);
                     return;
                 }
                 toast(tr("ui.toast.activated", {profileId}), false);
             });
}

void App::deleteProfile(const std::string& profileId)
{
    runAsync(tr("ui.busy.deleting"),
             [this, profileId]
             {
                 DaemonClient client = makeClient();
                 const Status status = client.deleteProfile(profileId);
                 if (!status.isOk())
                 {
                     toast(tr("ui.toast.deleteFailed", {status.error().describe()}), true);
                     return;
                 }
                 {
                     std::lock_guard<std::mutex> lock(mutex_);
                     if (editor_.originalId == profileId)
                     {
                         editor_ = ProfileEditor{};
                         ++editorGeneration_;
                     }
                 }
                 toast(tr("ui.toast.deleted", {profileId}), false);
             });
}

void App::openProfile(const std::string& profileId)
{
    runAsync(tr("ui.busy.loading"),
             [this, profileId]
             {
                 DaemonClient client = makeClient();
                 const auto profile = client.getProfile(profileId);
                 if (!profile)
                 {
                     toast(tr("ui.toast.loadFailed", {profile.error().describe()}), true);
                     return;
                 }

                 ProfileEditor editor;
                 editor.loaded = true;
                 editor.isNew = false;
                 editor.originalId = profile.value().id;
                 editor.profile = profile.value();
                 editor.hostlists = joinLines(editor.profile.hostlists);
                 editor.hostlistExcludes = joinLines(editor.profile.hostlistExcludes);
                 editor.ipsets = joinLines(editor.profile.ipsets);
                 editor.extraArguments = joinLines(editor.profile.extraArguments);
                 for (const DesyncRound& round : editor.profile.rounds)
                 {
                     RoundEditor roundEditor;
                     roundEditor.tcpText = serializePorts(round.filter.tcpPorts);
                     roundEditor.udpText = serializePorts(round.filter.udpPorts);
                     roundEditor.hostlists = joinLines(round.hostlists);
                     editor.rounds.push_back(std::move(roundEditor));
                 }
                 if (editor.rounds.empty())
                 {
                     editor.rounds.push_back(RoundEditor{});
                     editor.profile.rounds.push_back(DesyncRound{});
                 }

                 std::lock_guard<std::mutex> lock(mutex_);
                 editor_ = std::move(editor);
                 ++editorGeneration_;
             });
}

void App::beginNewProfile()
{
    Profile profile;
    profile.id = "new-profile";
    profile.name = "New profile";
    profile.tool = ToolKind::autoDetect;
    DesyncRound round;
    round.filter.tcpPorts = {443};
    round.modes = {DesyncMode::split2};
    round.splitPos = 1;
    profile.rounds.push_back(round);

    std::lock_guard<std::mutex> lock(mutex_);
    editor_ = ProfileEditor{};
    editor_.loaded = true;
    editor_.isNew = true;
    editor_.profile = std::move(profile);
    ++editorGeneration_;
    editor_.rounds.push_back(RoundEditor{"443", "", ""});
    previewCommands_.clear();
    previewDiagnostics_.clear();
    showPreview_ = false;
}

void App::saveEditorProfile()
{
    ProfileEditor editor;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        editor = editor_;
    }
    if (!editor.loaded)
    {
        return;
    }

    Profile profile = editor.profile;
    profile.hostlists = splitLines(editor.hostlists);
    profile.hostlistExcludes = splitLines(editor.hostlistExcludes);
    profile.ipsets = splitLines(editor.ipsets);
    profile.extraArguments = splitLines(editor.extraArguments);

    std::string parseError;
    for (std::size_t index = 0; index < profile.rounds.size(); ++index)
    {
        if (index >= editor.rounds.size())
        {
            break;
        }
        const std::string tcpError = strings::trim(editor.rounds[index].tcpText);
        const std::string udpError = strings::trim(editor.rounds[index].udpText);

        const auto tcpPorts = strings::parsePortList(tcpError);
        if (!tcpPorts.has_value())
        {
            parseError = "round " + std::to_string(index + 1) + ": invalid TCP port list";
            break;
        }
        const auto udpPorts = strings::parsePortList(udpError);
        if (!udpPorts.has_value())
        {
            parseError = "round " + std::to_string(index + 1) + ": invalid UDP port list";
            break;
        }
        profile.rounds[index].filter.tcpPorts = tcpPorts.value();
        profile.rounds[index].filter.udpPorts = udpPorts.value();
        profile.rounds[index].hostlists = splitLines(editor.rounds[index].hostlists);
    }

    if (!parseError.empty())
    {
        std::lock_guard<std::mutex> lock(mutex_);
        editor_.error = parseError;
        return;
    }

    const std::vector<Diagnostic> diagnostics = validateProfile(profile, 0);
    if (hasErrors(diagnostics))
    {
        std::lock_guard<std::mutex> lock(mutex_);
        editor_.error = strings::trim(formatDiagnostics(diagnostics));
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        editor_.error.clear();
        editor_.profile = profile;
    }

    runAsync(tr("ui.busy.saving"),
             [this, profile]
             {
                 DaemonClient client = makeClient();
                 const auto saved = client.saveProfile(profile);
                 if (!saved)
                 {
                     toast(tr("ui.toast.saveFailed", {saved.error().describe()}), true);
                     return;
                 }
                 {
                     std::lock_guard<std::mutex> lock(mutex_);
                     editor_.originalId = saved.value().id;
                     editor_.isNew = false;
                     ++editorGeneration_;
                 }
                 toast(tr("ui.toast.saved", {saved.value().id}), false);
             });
}

void App::requestPreview(const std::string& profileId)
{
    if (profileId.empty())
    {
        toast(tr("ui.toast.previewFirst"), true);
        return;
    }
    runAsync(tr("ui.busy.preview"),
             [this, profileId]
             {
                 DaemonClient client = makeClient();
                 const auto preview = client.previewProfile(profileId);
                 if (!preview)
                 {
                     toast(tr("ui.toast.previewFailed", {preview.error().describe()}), true);
                     return;
                 }

                 std::string diagnostics;
                 for (const Diagnostic& diagnostic : preview.value().diagnostics)
                 {
                     diagnostics += std::string{toString(diagnostic.severity)} + ": " + diagnostic.message + "\n";
                 }

                 std::lock_guard<std::mutex> lock(mutex_);
                 previewCommands_ = preview.value().commands;
                 previewDiagnostics_ = strings::trim(diagnostics);
                 showPreview_ = true;
             });
}

void App::runDoctor()
{
    runAsync(tr("ui.busy.diagnose"),
             [this]
             {
                 DaemonClient client = makeClient();
                 const auto health = client.health();
                 if (!health)
                 {
                     toast(tr("ui.toast.daemonUnreachable", {health.error().describe()}), true);
                     return;
                 }
                 const auto zapret = client.zapret();
                 if (!zapret)
                 {
                     toast(tr("ui.toast.zapretCheckFailed", {zapret.error().describe()}), true);
                     return;
                 }
                 if (!zapret.value().available)
                 {
                     toast(tr("ui.toast.zapretMissing"), true);
                     return;
                 }
                 toast(tr("ui.toast.diagnoseOk",
                          {health.value().version, strings::join(zapret.value().tools, ", ")}),
                       false);
             });
}

// ------------------------------------------------------------------ layout

void App::frame()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##deimos-root",
                 nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    renderLayout();
    renderToasts();

    ImGui::End();
}

void App::renderLayout()
{
    const Palette& colors = palette();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, colorVec4(colors.backgroundDark));
    ImGui::BeginChild("##sidebar", ImVec2(SIDEBAR_WIDTH, 0.0f), ImGuiChildFlags_None);
    renderSidebar();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, colorVec4(colors.background));
    ImGui::BeginChild("##content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    renderTopBar();

    switch (view_)
    {
    case View::dashboard:
        renderDashboard();
        break;
    case View::profiles:
        renderProfiles();
        break;
    case View::daemons:
        renderDaemons();
        break;
    case View::logs:
        renderLogs();
        break;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void App::renderSidebar()
{
    const Palette& colors = palette();
    ImGui::SetCursorPos(ImVec2(20.0f, 22.0f));

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    drawList->AddRectFilled(origin, ImVec2(origin.x + 34.0f, origin.y + 34.0f), colors.accentSoft, 10.0f);
    drawIcon(drawList, ImVec2(origin.x + 17.0f, origin.y + 17.0f), 18.0f, Icon::bolt, colors.accent);
    ImGui::Dummy(ImVec2(34.0f, 34.0f));
    ImGui::SameLine(46.0f);
    ImGui::BeginGroup();
    pushFont(fontHeading());
    ImGui::TextUnformatted(APP_NAME);
    popFont();
    mutedText(tr("ui.app.tagline").c_str());
    ImGui::EndGroup();

    spacer(28.0f);

    const std::pair<const char*, Icon> items[] = {
        {tr("ui.nav.dashboard").c_str(), Icon::dashboard},
        {tr("ui.nav.profiles").c_str(), Icon::profiles},
        {tr("ui.nav.daemons").c_str(), Icon::server},
        {tr("ui.nav.logs").c_str(), Icon::logs},
    };

    const float footerHeight = 140.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
    ImGui::SetCursorPosX(12.0f);
    // Bounded height: a full-height child would sit on top of the footer row and swallow its clicks.
    const float navHeight = std::max(0.0f, ImGui::GetContentRegionAvail().y - footerHeight);
    ImGui::BeginChild("##nav", ImVec2(SIDEBAR_WIDTH - 12.0f, navHeight));
    for (int index = 0; index < 4; ++index)
    {
        const bool selected = static_cast<int>(view_) == index;
        if (navItem(items[index].first, items[index].second, selected))
        {
            view_ = static_cast<View>(index);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    const float y = ImGui::GetWindowHeight() - footerHeight;
    if (y > 0.0f)
    {
        ImGui::SetCursorPos(ImVec2(12.0f, y));
        ImGui::BeginChild("##footer", ImVec2(SIDEBAR_WIDTH - 12.0f, footerHeight));

        ImGui::SetCursorPos(ImVec2(4.0f, 2.0f));

        const char* languageCodes[] = {"auto", "en", "ru"};
        const char* languageLabels[] = {
            tr("ui.language.auto").c_str(), tr("ui.language.en").c_str(), tr("ui.language.ru").c_str()};
        for (int index = 0; index < 3; ++index)
        {
            const ImVec2 buttonSize(66.0f, 26.0f);
            const bool selected = languageSetting_ == languageCodes[index];
            const bool clicked = selected ? primaryButton(languageLabels[index], buttonSize)
                                          : secondaryButton(languageLabels[index], buttonSize);
            if (clicked)
            {
                setLanguageSetting(languageCodes[index]);
            }
            if (index < 2)
            {
                ImGui::SameLine();
            }
        }

        ImGui::SetCursorPos(ImVec2(4.0f, 42.0f));
        const UiSnapshot current = snapshot();
        const ImU32 color = current.connected ? colors.success : colors.danger;

        ImDrawList* footerDraw = ImGui::GetWindowDrawList();
        const ImVec2 position = ImGui::GetCursorScreenPos();
        footerDraw->AddRectFilled(position,
                                  ImVec2(position.x + SIDEBAR_WIDTH - 40.0f, position.y + 74.0f),
                                  colors.panel,
                                  12.0f);
        footerDraw->AddCircleFilled(ImVec2(position.x + 18.0f, position.y + 22.0f), 4.0f, color);
        footerDraw->AddText(ImVec2(position.x + 32.0f, position.y + 14.0f),
                            colors.text,
                            tr(current.connected ? "ui.status.connected" : "ui.status.disconnected").c_str());
        const std::string endpointText = activeEndpoint().url;
        footerDraw->AddText(ImVec2(position.x + 18.0f, position.y + 40.0f), colors.textMuted, endpointText.c_str());
        if (!current.connected)
        {
            const std::string reason = current.error.empty() ? tr("ui.status.unreachable") : current.error;
            footerDraw->AddText(ImVec2(position.x + 18.0f, position.y + 56.0f), colors.danger, reason.c_str());
        }
        else
        {
            footerDraw->AddText(ImVec2(position.x + 18.0f, position.y + 56.0f),
                                colors.textMuted,
                                current.info.hostname.c_str());
        }
        ImGui::Dummy(ImVec2(SIDEBAR_WIDTH - 40.0f, 74.0f));
        ImGui::EndChild();
    }
}

void App::renderTopBar()
{
    const Palette& colors = palette();
    const UiSnapshot current = snapshot();

    ImGui::SetCursorPos(ImVec2(30.0f, 26.0f));
    ImGui::BeginGroup();
    pushFont(fontHeading());
    ImGui::TextUnformatted(titleFor(static_cast<int>(view_)).c_str());
    popFont();
    ImGui::EndGroup();

    switch (view_)
    {
    case View::dashboard:
        mutedText(tr("ui.subtitle.dashboard").c_str());
        break;
    case View::profiles:
        mutedText(tr("ui.subtitle.profiles").c_str());
        break;
    case View::daemons:
        mutedText(tr("ui.subtitle.daemons").c_str());
        break;
    case View::logs:
        mutedText(tr("ui.subtitle.logs").c_str());
        break;
    }

    const float rightEdge = ImGui::GetWindowWidth() - 30.0f;
    ImGui::SameLine(rightEdge - 250.0f);
    ImGui::BeginGroup();
    {
        const std::string label = isBusy() ? busyLabel() : std::string{};
        if (!label.empty())
        {
            mutedText(label.c_str());
        }
        else
        {
            mutedText(tr(current.connected ? "ui.status.synced" : "ui.status.offline").c_str());
        }
    }
    ImGui::EndGroup();

    ImGui::SameLine(rightEdge - 120.0f);
    if (iconButton("##refresh", Icon::refresh, tr("ui.action.refreshNow").c_str()))
    {
        refreshNow();
    }
    const std::string stateLabel{labels::engineState(current.info.engine.state)};
    const float stateWidth = ImGui::CalcTextSize(stateLabel.c_str()).x + 22.0f;
    ImGui::SameLine(rightEdge - stateWidth, 0.0f);
    pill(stateLabel.c_str(), stateColor(current.info.engine.state));

    spacer(18.0f);
    ImGui::PushStyleColor(ImGuiCol_Separator, colorVec4(colors.border));
    ImGui::Separator();
    ImGui::PopStyleColor();
    spacer(6.0f);
}

void App::renderToasts()
{
    std::deque<Toast> toasts;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        toasts = toasts_;
    }
    if (toasts.empty())
    {
        return;
    }

    const float delta = ImGui::GetIO().DeltaTime;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (Toast& toast : toasts_)
        {
            toast.remaining -= delta;
        }
        toasts_.erase(std::remove_if(toasts_.begin(),
                                     toasts_.end(),
                                     [](const Toast& toast)
                                     {
                                         return toast.remaining <= 0.0f;
                                     }),
                      toasts_.end());
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float offset = 24.0f;
    for (const Toast& toast : toasts)
    {
        const ImVec2 textSize = ImGui::CalcTextSize(toast.text.c_str());
        const ImVec2 size(textSize.x + 44.0f, textSize.y + 22.0f);
        const ImVec2 position(viewport->WorkPos.x + viewport->WorkSize.x - size.x - 28.0f,
                              viewport->WorkPos.y + viewport->WorkSize.y - size.y - offset);

        ImGui::SetNextWindowPos(position);
        ImGui::SetNextWindowSize(size);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, colorVec4(toast.isError ? IM_COL32(52, 24, 32, 245) : palette().panel));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));
        ImGui::Begin(("##toast" + toast.text).c_str(),
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted(toast.text.c_str());
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        offset += size.y + 10.0f;
    }
}

}
