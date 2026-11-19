<p align="center">
  <b>🇬🇧 English</b> &nbsp;·&nbsp; <a href="ru/strategies.md">🇷🇺 Русский</a>
</p>

# Strategies and zapret mapping

A *profile* is Deimos' portable description of a bypass strategy. It is intentionally
close to zapret's own vocabulary but never generates flags blindly: every profile can be
previewed before use.

```bash
deimos profiles preview tls-split
```

```text
C:/zapret/binaries/windows-x86_64/winws.exe --wf-tcp=443 --filter-tcp=443 --filter-l7=tls \
  --dpi-desync=split2 --dpi-desync-split-pos=1 --dpi-desync-autottl=2
```

## Rounds

A profile contains one or more *rounds*. Each round is a filter plus a desync
description, which is exactly zapret's `--new` separated rule set.

```json
{
  "filter": { "tcp": [443], "udp": [], "l7": ["tls"] },
  "modes": ["fake", "split2"],
  "splitPos": 1,
  "fooling": ["md5sig"],
  "autoTtl": 2,
  "repeats": 6
}
```

## Field mapping

| Profile field | winws / nfqws | tpws |
| --- | --- | --- |
| `filter.tcp` | `--filter-tcp=443` (winws also adds it to `--wf-tcp`) | `--port=443` |
| `filter.udp` | `--filter-udp=443` (winws also adds `--wf-udp`) | not supported |
| `filter.l7` | `--filter-l7=tls` | not supported |
| `modes` | `--dpi-desync=fake,split2` | `--split-pos`, `--disorder` |
| `splitPos` | `--dpi-desync-split-pos=1` | `--split-pos=1` |
| `splitSeqOverlap` | `--dpi-desync-split-seqovl=N` | warned, ignored |
| `fooling` | `--dpi-desync-fooling=md5sig` | warned, ignored |
| `ttl` | `--dpi-desync-ttl=N` | warned, ignored |
| `autoTtl` | `--dpi-desync-autottl=N` | warned, ignored |
| `repeats` | `--dpi-desync-repeats=N` | warned, ignored |
| `windowSize` | `--wssize=N` | not supported |
| `fakePayload` | `--dpi-desync-fake-tls=<file>` or `--dpi-desync-fake-tls=@0x<hex>` | n/a |
| `hostlists` | `--hostlist=<file>` (tpws: `--hosts`) | `--hosts` |
| `hostlistExcludes` | `--hostlist-exclude=<file>` | `--hostlist-exclude` |
| `ipsets` | `--ipset=<file>` | `--ipset` |
| `extraArguments` | appended verbatim | appended verbatim |

The fake payload option is chosen from the round's L7 filter: `quic` uses
`--dpi-desync-fake-quic`, `http` uses `--dpi-desync-fake-http`, `tls` uses
`--dpi-desync-fake-tls`, and a UDP-only round without an L7 filter defaults to QUIC.

## Process layout per tool

| Tool | Processes | Notes |
| --- | --- | --- |
| `winws` | one | Rounds are separated with `--new`; the WinDivert pre-filter (`--wf-tcp`/`--wf-udp`) is the union of all round ports unless the profile overrides it |
| `nfqws` | one per round | `--qnum` starts at `profile.nfqueue` and increments per round |
| `tpws` | one per round | TCP only; only split and disorder semantics exist |

`tool: "auto"` resolves to `winws` on Windows and `nfqws` elsewhere, falling back to any
detected tool.

## Validation

`buildLaunches()` validates every round before generating arguments, so the same rules
apply to the API preview, the CLI and the engine:

- a round needs at least one port;
- `ttl` must be 1..255, `autoTtl` 1..32, `repeats` 1..32;
- inline fake payloads must be even-length hex;
- `disorder` without fooling flags, TTL or autottl raises a warning, because the packet
  rarely reaches the server unmodified;
- more than two desync modes per round is suspicious;
- tpws rounds with fake modes, UDP ports or extra options produce errors or warnings;
- `--new` inside `extraArguments` is rejected, since rounds already produce it.

Severity `error` blocks a start; warnings are shown and ignored.

## Starter profiles

The generated configuration contains two profiles that work as a starting point:

| Id | Purpose |
| --- | --- |
| `tls-split` | Splits the TLS ClientHello on port 443 to defeat SNI inspection |
| `quic-split` | Answers QUIC Initial packets with a fake packet so broken video falls back to TCP |

They are deliberately conservative. Real-world DPI behaviour differs per ISP; use the
preview, the log stream and the `endpoints` of zapret's own presets to tune them.

## Where zapret is found

Detection tries, in order: `zapret.installDirectory`, `$DEIMOS_ZAPRET_DIR`, the directory
next to `deimosd`, the working directory, `%ProgramData%/zapret`,
`%ProgramFiles%/zapret`, `/opt/zapret`, `/usr/local/zapret`.

Inside a candidate tree Deimos looks at `bin/`, `binaries/<platform>/`, the root, and
finally a bounded depth-first scan. The tool names are `winws`, `nfqws` and `tpws`
(`winws.exe` on Windows). Relative hostlist paths are resolved against the zapret tree,
so a stock zapret layout with `lists/` and `files/` works without changes.
