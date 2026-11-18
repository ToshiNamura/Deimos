#include <deimos/model/desync.hpp>

#include <algorithm>
#include <array>

#include <deimos/model/diagnostic.hpp>
#include <deimos/support/strings.hpp>

namespace deimos
{
namespace
{

struct ToolKindEntry
{
    ToolKind value;
    std::string_view token;
    std::string_view display;
};

struct DesyncModeEntry
{
    DesyncMode value;
    std::string_view token;
    std::string_view display;
};

struct FoolingEntry
{
    Fooling value;
    std::string_view token;
    std::string_view display;
};

struct L7Entry
{
    L7Protocol value;
    std::string_view token;
    std::string_view display;
};

constexpr std::array<ToolKindEntry, 4> TOOL_KINDS{{
    {ToolKind::autoDetect, "auto", "Automatic"},
    {ToolKind::winws, "winws", "winws (WinDivert, Windows)"},
    {ToolKind::nfqws, "nfqws", "nfqws (NFQUEUE, Linux)"},
    {ToolKind::tpws, "tpws", "tpws (transparent proxy, Linux)"},
}};

constexpr std::array<DesyncModeEntry, 8> DESYNC_MODES{{
    {DesyncMode::split, "split", "Split"},
    {DesyncMode::split2, "split2", "Split (second burst)"},
    {DesyncMode::disorder, "disorder", "Disorder"},
    {DesyncMode::disorder2, "disorder2", "Disorder (second burst)"},
    {DesyncMode::fake, "fake", "Fake packet"},
    {DesyncMode::fakeSplit, "fakedsplit", "Fake + split"},
    {DesyncMode::fakeDisorder, "fakeddisorder", "Fake + disorder"},
    {DesyncMode::hostFakeSplit, "hostfakesplit", "Host-based fake split"},
}};

constexpr std::array<FoolingEntry, 6> FOOLING_FLAGS{{
    {Fooling::md5sig, "md5sig", "MD5 signature"},
    {Fooling::badseq, "badseq", "Bad sequence number"},
    {Fooling::badsum, "badsum", "Bad checksum"},
    {Fooling::datanoack, "datanoack", "Data without ACK"},
    {Fooling::hopbyhop, "hopbyhop", "IPv6 hop-by-hop"},
    {Fooling::hopbyhop2, "hopbyhop2", "IPv6 hop-by-hop (spoof)"},
}};

constexpr std::array<L7Entry, 4> L7_PROTOCOLS{{
    {L7Protocol::tls, "tls", "TLS"},
    {L7Protocol::http, "http", "HTTP"},
    {L7Protocol::quic, "quic", "QUIC"},
    {L7Protocol::unknown, "unknown", "Unknown L7"},
}};

/// Comparison ignores case, dashes and underscores so "fake-split" == "fakedsplit".
std::string normalizeKey(std::string_view text)
{
    std::string result = strings::toLower(strings::trim(text));
    result.erase(std::remove_if(result.begin(),
                                result.end(),
                                [](char character)
                                {
                                    return character == '-' || character == '_' || character == ' ' || character == '.';
                                }),
                 result.end());
    return result;
}

template <typename Entry, std::size_t Size, typename Enum>
bool parseEntry(const std::array<Entry, Size>& entries, std::string_view text, Enum& out)
{
    const std::string key = normalizeKey(text);
    for (const Entry& entry : entries)
    {
        if (key == entry.token || key == normalizeKey(entry.display))
        {
            out = entry.value;
            return true;
        }
    }
    return false;
}

template <typename Entry, std::size_t Size, typename Enum>
std::string_view tokenOf(const std::array<Entry, Size>& entries, Enum value)
{
    for (const Entry& entry : entries)
    {
        if (entry.value == value)
        {
            return entry.token;
        }
    }
    return {};
}

template <typename Entry, std::size_t Size, typename Enum>
std::string_view displayOf(const std::array<Entry, Size>& entries, Enum value)
{
    for (const Entry& entry : entries)
    {
        if (entry.value == value)
        {
            return entry.display;
        }
    }
    return {};
}

bool isFakeMode(DesyncMode mode)
{
    return mode == DesyncMode::fake || mode == DesyncMode::fakeSplit || mode == DesyncMode::fakeDisorder ||
           mode == DesyncMode::hostFakeSplit;
}

bool isDisorderMode(DesyncMode mode)
{
    return mode == DesyncMode::disorder || mode == DesyncMode::disorder2 || mode == DesyncMode::fakeDisorder;
}

bool containsMode(const std::vector<DesyncMode>& modes, DesyncMode mode)
{
    return std::find(modes.begin(), modes.end(), mode) != modes.end();
}

void reportInvalidHex(const std::string& value, const std::string& path, std::vector<Diagnostic>& diagnostics)
{
    if (value.empty())
    {
        diagnostics.push_back(error("round.fake_payload.empty", "inline fake payload is empty", path));
        return;
    }
    if (value.size() % 2 != 0)
    {
        diagnostics.push_back(error("round.fake_payload.odd_hex",
                                    "inline fake payload must contain an even number of hex digits",
                                    path));
        return;
    }
    if (!strings::fromHex(value).has_value())
    {
        diagnostics.push_back(
            error("round.fake_payload.invalid_hex", "inline fake payload contains non-hex characters", path));
    }
}

void checkDuplicatePorts(const std::vector<std::uint16_t>& ports,
                         std::string_view field,
                         const std::string& path,
                         std::vector<Diagnostic>& diagnostics)
{
    std::vector<std::uint16_t> seen;
    for (const std::uint16_t port : ports)
    {
        if (std::find(seen.begin(), seen.end(), port) != seen.end())
        {
            diagnostics.push_back(warning("round.filter.duplicate_port",
                                          std::string{"duplicate "} + std::string{field} + " port " +
                                              std::to_string(port),
                                          path));
            continue;
        }
        seen.push_back(port);
    }
}

} // namespace

std::string_view toCliToken(ToolKind value)
{
    return tokenOf(TOOL_KINDS, value);
}

std::string_view toCliToken(DesyncMode value)
{
    return tokenOf(DESYNC_MODES, value);
}

std::string_view toCliToken(Fooling value)
{
    return tokenOf(FOOLING_FLAGS, value);
}

std::string_view toCliToken(L7Protocol value)
{
    return tokenOf(L7_PROTOCOLS, value);
}

std::string_view toCliToken(FakePayloadKind value)
{
    return value == FakePayloadKind::hex ? "hex" : "file";
}

std::string_view toDisplayName(ToolKind value)
{
    return displayOf(TOOL_KINDS, value);
}

std::string_view toDisplayName(DesyncMode value)
{
    return displayOf(DESYNC_MODES, value);
}

std::string_view toDisplayName(Fooling value)
{
    return displayOf(FOOLING_FLAGS, value);
}

std::string_view toDisplayName(L7Protocol value)
{
    return displayOf(L7_PROTOCOLS, value);
}

bool parseToolKind(std::string_view text, ToolKind& out)
{
    return parseEntry(TOOL_KINDS, text, out);
}

bool parseDesyncMode(std::string_view text, DesyncMode& out)
{
    return parseEntry(DESYNC_MODES, text, out);
}

bool parseFooling(std::string_view text, Fooling& out)
{
    return parseEntry(FOOLING_FLAGS, text, out);
}

bool parseL7Protocol(std::string_view text, L7Protocol& out)
{
    return parseEntry(L7_PROTOCOLS, text, out);
}

bool parseFakePayloadKind(std::string_view text, FakePayloadKind& out)
{
    const std::string key = normalizeKey(text);
    if (key == "file" || key == "path")
    {
        out = FakePayloadKind::file;
        return true;
    }
    if (key == "hex" || key == "inline")
    {
        out = FakePayloadKind::hex;
        return true;
    }
    return false;
}

const std::vector<ToolKind>& allToolKinds()
{
    static const std::vector<ToolKind> values{ToolKind::autoDetect, ToolKind::winws, ToolKind::nfqws, ToolKind::tpws};
    return values;
}

const std::vector<DesyncMode>& allDesyncModes()
{
    static const std::vector<DesyncMode> values{
        DesyncMode::split,
        DesyncMode::split2,
        DesyncMode::disorder,
        DesyncMode::disorder2,
        DesyncMode::fake,
        DesyncMode::fakeSplit,
        DesyncMode::fakeDisorder,
        DesyncMode::hostFakeSplit,
    };
    return values;
}

const std::vector<Fooling>& allFoolingFlags()
{
    static const std::vector<Fooling> values{
        Fooling::md5sig,
        Fooling::badseq,
        Fooling::badsum,
        Fooling::datanoack,
        Fooling::hopbyhop,
        Fooling::hopbyhop2,
    };
    return values;
}

const std::vector<L7Protocol>& allL7Protocols()
{
    static const std::vector<L7Protocol> values{L7Protocol::tls, L7Protocol::http, L7Protocol::quic, L7Protocol::unknown};
    return values;
}

bool DesyncRound::usesFakePayload() const
{
    return std::any_of(modes.begin(), modes.end(), isFakeMode);
}

std::vector<Diagnostic> validateRound(const DesyncRound& round, std::string_view path, ToolKind tool)
{
    std::vector<Diagnostic> diagnostics;
    const std::string base{path};

    if (round.filter.isEmpty())
    {
        diagnostics.push_back(error("round.filter.empty",
                                    "a desync round needs at least one TCP or UDP port",
                                    base + ".filter"));
    }

    if (round.modes.empty())
    {
        diagnostics.push_back(error("round.modes.empty",
                                    "a desync round needs at least one desync mode",
                                    base + ".modes"));
    }

    checkDuplicatePorts(round.filter.tcpPorts, "TCP", base + ".filter.tcp", diagnostics);
    checkDuplicatePorts(round.filter.udpPorts, "UDP", base + ".filter.udp", diagnostics);

    if (round.splitPos.has_value() && *round.splitPos < 0)
    {
        diagnostics.push_back(error("round.split_pos.negative",
                                    "split position cannot be negative",
                                    base + ".splitPos"));
    }

    if (round.splitSeqOverlap.has_value() && *round.splitSeqOverlap < 0)
    {
        diagnostics.push_back(error("round.split_seqovl.negative",
                                    "sequence overlap cannot be negative",
                                    base + ".splitSeqOverlap"));
    }

    if (round.ttl.has_value() && (*round.ttl <= 0 || *round.ttl > 255))
    {
        diagnostics.push_back(error("round.ttl.range", "TTL must be within 1..255", base + ".ttl"));
    }

    if (round.autoTtl.has_value() && (*round.autoTtl <= 0 || *round.autoTtl > 32))
    {
        diagnostics.push_back(error("round.autottl.range",
                                    "automatic TTL correction must be within 1..32",
                                    base + ".autoTtl"));
    }

    if (round.repeats.has_value() && (*round.repeats <= 0 || *round.repeats > 32))
    {
        diagnostics.push_back(error("round.repeats.range", "repeat count must be within 1..32", base + ".repeats"));
    }

    if (round.windowSize.has_value() && *round.windowSize < 0)
    {
        diagnostics.push_back(error("round.window_size.negative",
                                    "window size cannot be negative",
                                    base + ".windowSize"));
    }

    if (round.fakePayload.has_value())
    {
        if (round.fakePayload->value.empty())
        {
            diagnostics.push_back(error("round.fake_payload.empty",
                                        "fake payload reference is empty",
                                        base + ".fakePayload"));
        }
        else if (round.fakePayload->kind == FakePayloadKind::hex)
        {
            reportInvalidHex(round.fakePayload->value, base + ".fakePayload", diagnostics);
        }
    }
    else if (round.usesFakePayload())
    {
        diagnostics.push_back(info("round.fake_payload.default",
                                   "no fake payload configured, zapret will use its built-in default",
                                   base + ".fakePayload"));
    }

    const bool hasSpoofingHint = !round.fooling.empty() || round.ttl.has_value() || round.autoTtl.has_value();
    if (std::any_of(round.modes.begin(), round.modes.end(), isDisorderMode) && !hasSpoofingHint)
    {
        diagnostics.push_back(warning("round.disorder.no_spoofing",
                                      "disorder desync normally needs fooling flags or a TTL to reach the server",
                                      base + ".fooling"));
    }

    if (round.modes.size() > 2)
    {
        diagnostics.push_back(warning("round.modes.too_many",
                                      "more than two desync modes per round is rarely useful",
                                      base + ".modes"));
    }

    if (tool == ToolKind::tpws)
    {
        const bool hasFake = containsMode(round.modes, DesyncMode::fake) ||
                             containsMode(round.modes, DesyncMode::fakeSplit) ||
                             containsMode(round.modes, DesyncMode::fakeDisorder) ||
                             containsMode(round.modes, DesyncMode::hostFakeSplit);
        if (hasFake)
        {
            diagnostics.push_back(error("round.tpws.unsupported_mode",
                                        "tpws cannot send fake packets; use split or disorder instead",
                                        base + ".modes"));
        }
        if (round.fooling.size() > 0 || round.ttl.has_value() || round.repeats.has_value())
        {
            diagnostics.push_back(warning("round.tpws.unsupported_options",
                                          "tpws ignores fooling, TTL and repeats",
                                          base + ".modes"));
        }
        if (!round.filter.udpPorts.empty())
        {
            diagnostics.push_back(error("round.tpws.udp_not_supported",
                                        "tpws only handles TCP; remove UDP ports from this round",
                                        base + ".filter.udp"));
        }
        if (round.filter.tcpPorts.empty())
        {
            diagnostics.push_back(error("round.tpws.tcp_required",
                                        "tpws rounds need at least one TCP port",
                                        base + ".filter.tcp"));
        }
    }

    if (tool == ToolKind::winws || tool == ToolKind::nfqws)
    {
        if (round.filter.tcpPorts.empty() && round.filter.udpPorts.empty())
        {
            diagnostics.push_back(error("round.filter.empty", "no ports selected", base + ".filter"));
        }
    }

    return diagnostics;
}

std::string serializeDesyncModes(const std::vector<DesyncMode>& modes)
{
    std::vector<std::string> tokens;
    tokens.reserve(modes.size());
    for (const DesyncMode mode : modes)
    {
        tokens.emplace_back(toCliToken(mode));
    }
    return strings::join(tokens, ",");
}

std::string serializeFooling(const std::vector<Fooling>& flags)
{
    std::vector<std::string> tokens;
    tokens.reserve(flags.size());
    for (const Fooling flag : flags)
    {
        tokens.emplace_back(toCliToken(flag));
    }
    return strings::join(tokens, ",");
}

std::string serializeL7(const std::vector<L7Protocol>& protocols)
{
    std::vector<std::string> tokens;
    tokens.reserve(protocols.size());
    for (const L7Protocol protocol : protocols)
    {
        tokens.emplace_back(toCliToken(protocol));
    }
    return strings::join(tokens, ",");
}

std::string serializePorts(const std::vector<std::uint16_t>& ports)
{
    std::vector<std::string> tokens;
    tokens.reserve(ports.size());
    for (const std::uint16_t port : ports)
    {
        tokens.emplace_back(std::to_string(port));
    }
    return strings::join(tokens, ",");
}

}
