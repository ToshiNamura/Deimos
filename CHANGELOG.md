# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-11-18

First public release.

### Added

- `deimosd`, a headless daemon that owns the zapret child processes: per-profile launch
  specs, restart policy (`engine.maxRestarts`, `engine.restartBackoffMs`) and output
  forwarding into the daemon log.
- Configuration store with atomic writes (temporary file plus rename), schema versioning
  and validation diagnostics that name the offending field.
- REST API under `/api/v1` for status, configuration, profiles, engine control, logs and
  command previews, with structured error codes and bearer-token authentication.
- zapret toolchain detection for `winws`, `nfqws` and `tpws`, including
  `zapret.installDirectory`, tool overrides and reporting through `deimos doctor`.
- Portable profile model that maps filters, desync modes, split positions, fooling, fake
  payloads, host lists and ipsets onto each tool's command line, with a preview endpoint
  and CLI command.
- Process supervision: stdout/stderr capture with partial-line reassembly, Windows job
  objects with `KILL_ON_JOB_CLOSE`, POSIX process groups with graceful shutdown.
- `deimos`, a single client binary: ImGui desktop interface and a full command line client
  over the same transport, with saved endpoints in `cli.json` and `ui.json`.
- Service integration: `--install-service` for Windows services and systemd,
  `--print-unit`, `--print-plist` and `--generate-token`.
- CMake presets (`dev`, `release`, `daemon-only`, `windows-vs2022`, `windows-vs2026`),
  component-based install rules and CPack packaging (ZIP/NSIS on Windows, TGZ/DEB
  elsewhere).
- Catch2 test suite covering configuration round-trips and validation, protocol
  serialisation, zapret command generation, process supervision and the HTTP API.
- Documentation for architecture, the daemon API, strategies, building and deployment.

[Unreleased]: https://github.com/ToshiNamura/Deimos/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/ToshiNamura/Deimos/releases/tag/v0.1.0
