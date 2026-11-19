# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- English and Russian localisation for the desktop interface, the CLI, both command line
  helps, the daemon logs, the REST API error payloads and every validation diagnostic.
  The language follows `DEIMOS_LANG`, `LC_ALL`/`LC_MESSAGES`/`LANG` or the system UI
  language, and can be pinned per process with `--lang auto|en|ru`. The interface switches
  language without a restart and remembers the choice in `ui.json`.
- `daemon.language` (`auto`, `en`, `ru`) pins the language of the daemon's diagnostics and
  logs; `deimosd --lang` overrides it for one run.
- Project assets: `assets/logo.png`, `assets/icon.png` (283x300) and
  `assets/background.png` (1200x300), with `assets/generate-assets.ps1` to regenerate them.
- [docs/localization.md](docs/localization.md), a guide to the message catalog.

### Fixed

- The engine state pill in the top bar showed the raw internal state instead of its
  translated label.

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
- CMake presets (`x64-debug`, `x64-release`, `linux-debug`, `linux-release`,
  `macos-debug`, `macos-release`, `daemon-only`), component-based install rules and CPack
  packaging (ZIP/NSIS on Windows, TGZ/DEB elsewhere).
- Catch2 test suite covering configuration round-trips and validation, protocol
  serialisation, zapret command generation, process supervision and the HTTP API.
- Documentation for architecture, the daemon API, strategies, building and deployment.

[Unreleased]: https://github.com/ToshiNamura/Deimos/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/ToshiNamura/Deimos/releases/tag/v0.1.0
