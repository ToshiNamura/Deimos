#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <deimos/model/diagnostic.hpp>

namespace deimos
{

/// zapret ships three different desync engines; a profile targets one of them.
enum class ToolKind : std::uint8_t
{
    autoDetect = 0,
    winws,
    nfqws,
    tpws,
};

/// Desync techniques accepted by --dpi-desync.
enum class DesyncMode : std::uint8_t
{
    split = 0,
    split2,
    disorder,
    disorder2,
    fake,
    fakeSplit,
    fakeDisorder,
    hostFakeSplit,
};

/// Values accepted by --dpi-desync-fooling.
enum class Fooling : std::uint8_t
{
    md5sig = 0,
    badseq,
    badsum,
    datanoack,
    hopbyhop,
    hopbyhop2,
};

/// Values accepted by --filter-l7.
enum class L7Protocol : std::uint8_t
{
    tls = 0,
    http,
    quic,
    unknown,
};

/// Where a fake payload comes from.
enum class FakePayloadKind : std::uint8_t
{
    /// A file shipped with zapret, for example files/fake/tls_clienthello_www_google_com.bin.
    file = 0,
    /// An inline hex blob passed to zapret as @0x....
    hex,
};

std::string_view toCliToken(ToolKind value);
std::string_view toCliToken(DesyncMode value);
std::string_view toCliToken(Fooling value);
std::string_view toCliToken(L7Protocol value);
std::string_view toCliToken(FakePayloadKind value);

std::string_view toDisplayName(ToolKind value);
std::string_view toDisplayName(DesyncMode value);
std::string_view toDisplayName(Fooling value);
std::string_view toDisplayName(L7Protocol value);

bool parseToolKind(std::string_view text, ToolKind& out);
bool parseDesyncMode(std::string_view text, DesyncMode& out);
bool parseFooling(std::string_view text, Fooling& out);
bool parseL7Protocol(std::string_view text, L7Protocol& out);
bool parseFakePayloadKind(std::string_view text, FakePayloadKind& out);

/// All enum values, used by the UI to build pickers and by docs generators.
const std::vector<ToolKind>& allToolKinds();
const std::vector<DesyncMode>& allDesyncModes();
const std::vector<Fooling>& allFoolingFlags();
const std::vector<L7Protocol>& allL7Protocols();

struct FakePayload
{
    FakePayloadKind kind = FakePayloadKind::file;
    std::string value;

    bool operator==(const FakePayload& other) const = default;
};

/// Ports and protocol selectors for one desync round.
struct FilterSpec
{
    std::vector<std::uint16_t> tcpPorts;
    std::vector<std::uint16_t> udpPorts;
    std::vector<L7Protocol> l7;

    bool isEmpty() const
    {
        return tcpPorts.empty() && udpPorts.empty();
    }

    bool operator==(const FilterSpec& other) const = default;
};

/// One --new separated rule group inside a profile.
struct DesyncRound
{
    FilterSpec filter;
    std::vector<DesyncMode> modes;

    std::optional<int> splitPos;
    std::optional<int> splitSeqOverlap;
    std::vector<Fooling> fooling;
    std::optional<int> ttl;
    std::optional<int> autoTtl;
    std::optional<int> repeats;
    std::optional<int> windowSize;
    std::optional<FakePayload> fakePayload;

    std::vector<std::string> hostlists;
    std::vector<std::string> hostlistExcludes;
    std::vector<std::string> ipsets;

    bool usesFakePayload() const;

    bool operator==(const DesyncRound& other) const = default;
};

/// Validates one round in isolation and returns diagnostics with \p path prefix.
std::vector<Diagnostic> validateRound(const DesyncRound& round, std::string_view path, ToolKind tool);

/// "fake,split2" - the exact token zapret expects for --dpi-desync.
std::string serializeDesyncModes(const std::vector<DesyncMode>& modes);

/// "md5sig,badseq" - the exact token zapret expects for --dpi-desync-fooling.
std::string serializeFooling(const std::vector<Fooling>& flags);

/// "tls,http" - the exact token zapret expects for --filter-l7.
std::string serializeL7(const std::vector<L7Protocol>& protocols);

/// "80,443" - the exact token zapret expects for port filters.
std::string serializePorts(const std::vector<std::uint16_t>& ports);

}
