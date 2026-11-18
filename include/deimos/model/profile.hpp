#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <deimos/model/desync.hpp>
#include <deimos/model/diagnostic.hpp>
#include <deimos/support/error.hpp>

namespace deimos
{

/// A named set of zapret rules the user can activate on a daemon.
struct Profile
{
    /// Slug used in URLs and on the command line, for example "youtube-quic".
    std::string id;
    std::string name;
    std::string description;
    ToolKind tool = ToolKind::autoDetect;

    /// --qnum for nfqws; incremented per round when several rounds are defined.
    std::uint16_t nfqueue = 200;

    /// WinDivert pre-filter for winws. When empty it is derived from the rounds.
    std::vector<std::uint16_t> winDivertTcpPorts;
    std::vector<std::uint16_t> winDivertUdpPorts;

    std::vector<DesyncRound> rounds;

    /// Applied to every round in addition to per-round lists.
    std::vector<std::string> hostlists;
    std::vector<std::string> hostlistExcludes;
    std::vector<std::string> ipsets;

    /// Appended verbatim, for flags Deimos does not model yet.
    std::vector<std::string> extraArguments;

    bool enabled = true;
    std::string notes;
};

/// Lowercases, replaces unsupported characters with '-' and collapses dashes.
std::string slugify(std::string_view text);

/// Validates one profile. \p index is used to build diagnostic paths.
std::vector<Diagnostic> validateProfile(const Profile& profile, std::size_t index);

std::string serializeProfile(const Profile& profile, bool pretty = true);
Status parseProfile(std::string_view text, Profile& out, std::vector<Diagnostic>& diagnostics);

/// Compact representation used by list endpoints.
struct ProfileSummary
{
    std::string id;
    std::string name;
    ToolKind tool = ToolKind::autoDetect;
    bool enabled = true;
    std::size_t roundCount = 0;
    std::string description;
};

std::string serializeProfileSummary(const ProfileSummary& summary, bool pretty = true);
std::string serializeProfileSummaries(const std::vector<ProfileSummary>& summaries, bool pretty = true);

ProfileSummary summarize(const Profile& profile);

}
