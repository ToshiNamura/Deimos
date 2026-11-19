# Architecture

Deimos is a control plane for DPI bypass. It does not implement packet editing itself:
it drives [zapret](https://github.com/bol-van/zapret), which owns the actual
desynchronisation logic.

```
                    +-------------------------------------------+
                    |              deimos (client)              |
                    |  CLI  ----------+                         |
                    |  GUI  ----------+--> DaemonClient (HTTP)  |
                    +---------------------------|---------------+
                                                | JSON over HTTP
                                   +------------v-------------+
                                   |          deimosd         |
                                   |  API server              |
                                   |  ConfigStore             |
                                   |  RuntimeEngine           |
                                   |    Process --> winws     |
                                   |               nfqws      |
                                   |               tpws       |
                                   +--------------------------+
```

## Components

| Component | Target | Responsibility |
| --- | --- | --- |
| `deimos_core` | static library | Configuration model, zapret strategy model and command generation, HTTP transport, daemon client, process supervisor, message catalog |
| `deimos_daemon` | static library | Runtime engine, configuration store, REST API, platform service integration |
| `deimosd` | executable | Daemon entry point |
| `deimos` | executable | Single client binary: GUI by default without arguments, CLI with arguments |
| `deimos_tests` | executable | Catch2 test suite |

Everything shared lives in `deimos_core`, so the CLI and the GUI use exactly the same
protocol, configuration parsing and endpoints. There is no duplicated logic between the
two front ends - they are two presentations of one binary.

## One binary, two modes

`deimos` is a console-subsystem executable. That keeps pipes, exit codes and blocking
behaviour correct for scripting: `deimos status --json | ConvertFrom-Json` works.

Mode selection in `src/client/main.cpp`:

| Invocation | Result |
| --- | --- |
| `deimos status`, `deimos --help`, any argument | CLI |
| `deimos --gui` | GUI, always |
| `deimos` with no arguments from a terminal | CLI, prints help |
| `deimos` with no arguments from Explorer or a desktop entry | GUI |

The terminal check uses `GetConsoleProcessList` on Windows (a console shared with a
parent shell means "launched from a terminal") and `isatty` elsewhere. Before opening
the window the process calls `FreeConsole()`, so a double click does not leave a stray
console window behind.

## Configuration flow

1. `deimosd` loads `<config>/deimos.json` (defaults are written on first run).
2. Every profile is validated; diagnostics are returned to clients instead of silently
   dropping invalid rules.
3. `PUT /api/v1/config` or a profile write updates the file atomically (temporary file
   plus rename), then the daemon re-detects zapret and restarts the engine if it was
   running.

The schema is versioned (`schemaVersion`). A daemon refuses a configuration written by a
newer schema instead of guessing.

## Engine

`RuntimeEngine` owns the zapret child processes for one active profile:

- `Profile` -> `LaunchSpec` list via `buildLaunches()`.
  `winws` gets a single process with `--new` separators between rounds, because that is
  how zapret's Windows tool multiplexes rule sets. `nfqws` and `tpws` accept one rule
  set per invocation, so Deimos starts one process per round with incremented `--qnum`.
- Each process is spawned with captured stdout/stderr. Lines are forwarded to the log
  under `zapret/<profile>` and to a partial-line buffer per process, so a chunk split in
  the middle of a line is never cut in the log.
- A monitor thread polls the children (`Process::poll()`), applies the restart policy
  from `engine.maxRestarts` / `engine.restartBackoffMs`, and moves the engine to
  `faulted` with a `lastError` when restarts are exhausted or disabled.
- On Windows children are placed in a job object with `KILL_ON_JOB_CLOSE`, so they never
  survive a crashed daemon. On POSIX each child gets its own process group and receives
  `SIGTERM`, then `SIGKILL` after the grace period.

## Threading

| Thread | Owns |
| --- | --- |
| API server workers (cpp-httplib pool) | Request handling; all shared state is guarded by `ConfigStore` and `RuntimeEngine` mutexes |
| Engine monitor | Child exit detection and restarts |
| Process reader threads (one per stream) | Draining stdout/stderr into the log |
| GUI poll thread | Periodically fetching status, profiles and logs |
| GUI worker thread | Serialised actions (start/stop/save/delete) so the UI never blocks on the network |

The engine uses two mutexes: `mutex_` for state and `outputMutex_` for the partial line
buffers. Output callbacks never take the state mutex, which is what makes stopping a
process while its reader thread is mid-write safe.

## Security model

- The daemon binds `127.0.0.1` by default. Listening on any other address requires both
  `allowRemoteClients` and `requireAuthentication`; the configuration is rejected
  otherwise.
- Every endpoint except `/api/v1/health` requires `Authorization: Bearer <token>` when
  authentication is enabled. Comparison is constant time.
- The token lives in `<state>/token` with owner-only permissions and is printed once by
  `deimosd --generate-token`.
- TLS (`enableTls` plus a certificate and key) is optional and compiled in with
  `-DDEIMOS_ENABLE_TLS=ON`. For internet-facing deployments put the daemon behind a
  reverse proxy or an SSH tunnel instead of exposing it directly.
- zapret itself needs raw packet access: run the daemon as a service with
  `CAP_NET_ADMIN`/`CAP_NET_RAW` or install the Windows service from an elevated prompt.

## Language and diagnostics

Every message the daemon produces - validation diagnostics, API error payloads and its own
log lines - is translated. The language comes from `--lang`, `daemon.language` or the
system UI language; see [Localisation](localization.md). The catalog lives in
`src/core/support/i18n_catalog.inc` and is compiled into both binaries, so a daemon on a
router and the client on a desktop share the same strings without shipping data files.

The client keeps its own language: a Russian `deimos` talking to an English `deimosd`
renders its interface in Russian and shows the daemon's messages in English. Set
`daemon.language` on the daemon if a single language is required end to end.

## Why not link against zapret?

zapret is a set of standalone tools with a stable command line and no embeddable
library API. Deimos treats them as an external dependency: the same binaries the user
already trusts, no patching, no vendoring, and the exact commands are visible through
`GET /api/v1/profiles/{id}/preview` before anything is started.
