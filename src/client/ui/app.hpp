#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <deimos/model/config.hpp>
#include <deimos/model/profile.hpp>
#include <deimos/transport/daemon_client.hpp>
#include <deimos/transport/protocol.hpp>

namespace deimos::ui
{

struct EndpointRecord
{
    std::string name;
    std::string url{"http://127.0.0.1:8787"};
    std::string tokenFile;
    std::string token;
};

struct UiSnapshot
{
    bool connected = false;
    std::string error;
    DaemonInfo info;
    std::vector<ProfileSummary> profiles;
    std::vector<LogEntry> logs;
    std::chrono::system_clock::time_point fetchedAt{};
};

struct Toast
{
    std::string text;
    bool isError = false;
    float remaining = 4.0f;
};

struct RoundEditor
{
    std::string tcpText;
    std::string udpText;
    std::string hostlists;
};

struct ProfileEditor
{
    bool loaded = false;
    std::string originalId;
    Profile profile;
    std::vector<RoundEditor> rounds;
    std::string hostlists;
    std::string hostlistExcludes;
    std::string ipsets;
    std::string extraArguments;
    std::string error;
    bool isNew = false;
};

class App
{
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool initialize();
    void frame();
    void shutdown();

    /// Language requested on the command line; wins over the stored preference.
    void setLanguageOverride(std::string code);

private:
    enum class View
    {
        dashboard,
        profiles,
        daemons,
        logs,
    };

    void renderLayout();
    void renderSidebar();
    void renderTopBar();
    void renderDashboard();
    void renderProfiles();
    void renderDaemons();
    void renderLogs();
    void renderToasts();

    UiSnapshot snapshot() const;
    EndpointRecord activeEndpoint() const;
    void selectEndpoint(const std::string& name);
    void setLanguageSetting(const std::string& code);
    void loadState();
    void persistState();

    DaemonClient makeClient() const;

    void pollLoop();
    void pollOnce();
    void runAsync(const std::string& label, std::function<void()> task);
    void toast(std::string text, bool isError);
    bool isBusy() const;
    std::string busyLabel() const;

    void startEngine(const std::string& profileId);
    void stopEngine();
    void restartEngine();
    void activateProfile(const std::string& profileId);
    void saveEditorProfile();
    void deleteProfile(const std::string& profileId);
    void openProfile(const std::string& profileId);
    void beginNewProfile();
    void requestPreview(const std::string& profileId);
    void runDoctor();
    void refreshNow();

    // --- state guarded by mutex_
    mutable std::mutex mutex_;
    UiSnapshot snapshot_;
    View view_ = View::dashboard;
    std::vector<EndpointRecord> endpoints_;
    std::string currentEndpoint_;
    ProfileEditor editor_;
    std::vector<std::string> previewCommands_;
    std::string previewDiagnostics_;
    std::deque<Toast> toasts_;
    std::string busyLabel_;
    int pendingActions_ = 0;
    bool showPreview_ = false;

    /// Bumped whenever a background action replaces editor_, so the view does not
    /// copy its stale frame back over the fresh data.
    unsigned long long editorGeneration_ = 0;

    std::atomic<bool> running_{false};
    std::atomic<bool> refreshRequested_{true};
    std::atomic<int> refreshSeconds_{2};

    std::thread pollThread_;
    std::thread worker_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<std::pair<std::string, std::function<void()>>> queue_;
    bool workerRunning_ = false;

    // UI-only form state
    EndpointRecord newEndpoint_;
    char newEndpointName_[64] = {};
    char newEndpointUrl_[256] = {};
    char newEndpointToken_[256] = {};
    std::string logsFilter_;
    std::string languageSetting_{"auto"};
    std::string languageOverride_;
    bool logsAutoScroll_ = true;
};

}
