# Deimos

Deimos is a cross-platform control plane for DPI bypass on top of
[zapret](https://github.com/bol-van/zapret) and [zapret2](https://github.com/bol-van/zapret2).

It splits the problem into two programs:

| Program | Role |
| --- | --- |
| `deimosd` | Headless daemon. Owns the zapret processes, applies profiles, serves the management API. Runs locally, on a router, or on a VPS. |
| `deimos` | One client binary: opens the desktop interface when started without arguments, and behaves as a full command line client when given a command. |

The daemon and the client speak plain HTTP + JSON with a bearer token, so the same UI and
CLI manage a daemon on `127.0.0.1` and a daemon on a remote server.

## Status

Early development. The architecture, API and configuration format are in place; the UI and
CLI are functional but still growing.

## Documentation

- [Architecture](docs/architecture.md) - components, data flow, threading model, security
- [Daemon API](docs/daemon-api.md) - REST reference with examples
- [Strategies](docs/strategies.md) - how profiles map onto zapret arguments
- [Building](docs/building.md) - toolchains, presets, packaging
- [Deployment](docs/deployment.md) - services on Windows, Linux and macOS

## Quick start

### Build everything

```powershell
cmake --preset windows-vs2026
cmake --build --preset windows-vs2026
```

```bash
cmake --preset release
cmake --build --preset release
```

### Build only the daemon (servers, routers, headless machines)

```bash
cmake --preset daemon-only
cmake --build --preset daemon-only
```

`daemon-only` produces a single `deimosd` binary with a static C++ runtime and no UI, CLI
or test dependencies. `-DDEIMOS_SERVER_ONLY=ON` is the equivalent switch.

### Run a daemon

```bash
deimosd --generate-token        # writes <state dir>/token, prints it once
deimosd --config /etc/deimos/deimos.json
```

### Open the desktop interface

```bash
deimos --gui
```

### Use the command line

```bash
export DEIMOS_TOKEN=$(cat ~/.local/state/deimos/token)
deimos status
deimos profiles list
deimos start --profile tls-split
deimos logs --limit 50
deimos doctor
```

## The client binary

`deimos` is a single console binary that serves both roles:

| Invocation | Behaviour |
| --- | --- |
| `deimos <command> ...` | command line client, exit codes and pipes work as usual |
| `deimos --gui` | desktop interface |
| `deimos` from a terminal | prints help |
| `deimos` from Explorer or a desktop entry | desktop interface |

Clients keep their endpoints in `cli.json` / `ui.json` next to the configuration, so a GUI
user can manage several daemons and the CLI can pick the same one with `endpoints use`.

## Requirements

- CMake 3.25 or newer
- A C++20 compiler (MSVC 19.4x, GCC 12+, Clang 15+)
- zapret or zapret2 binaries for the platform whose traffic you want to modify
- An OpenGL 3.2 driver for the GUI (the daemon and CLI are headless)

Third-party dependencies (nlohmann/json, cpp-httplib, GLFW, Dear ImGui, Catch2) are
downloaded automatically by CMake unless they are already installed.

## License

MIT, see [LICENSE](LICENSE). zapret is a separate project with its own license and is never
bundled.
