<p align="center">
  <b>🇬🇧 English</b> &nbsp;·&nbsp; <a href="ru/building.md">🇷🇺 Русский</a>
</p>

# Building

## Requirements

- CMake 3.25 or newer
- A C++20 compiler: MSVC 19.4x (Visual Studio 2022/2026), GCC 12+, Clang 15+
- Network access on the first configure, or the dependencies installed system-wide
  (nlohmann/json, cpp-httplib, GLFW, Dear ImGui, Catch2)

The GUI needs an OpenGL 3.2 capable driver. Everything else is headless.

## Presets

The presets use Ninja with the platform compiler, so they expect a developer shell
(`vcvars64.bat` on Windows, a normal shell with a compiler on Linux and macOS):

| Preset | What it configures |
| --- | --- |
| `x64-debug`, `x64-release` | Windows, Ninja with `cl.exe`, Debug or Release |
| `linux-debug`, `linux-release` | Linux, Ninja, Debug or Release |
| `macos-debug`, `macos-release` | macOS, Ninja, Debug or Release |
| `daemon-only` | `deimosd` only, static runtime, no UI/CLI/tests, any platform |

The platform presets are conditioned on the host, so `x64-release` only configures on
Windows and `linux-debug` only on Linux. Builds land in `out/build/<preset>` and installs
in `out/install/<preset>`:

```bash
cmake --preset linux-debug     # Ninja, Debug, tests
cmake --preset linux-release   # Ninja, Release, tests
cmake --preset daemon-only     # deimosd only, static runtime, no UI/CLI/tests
```

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
```

On Linux the GUI needs the X11 development headers; CI passes `-DGLFW_BUILD_WAYLAND=OFF`
to GLFW so the configure step does not require `wayland-scanner`. Drop the flag if the
toolchain is installed and you want native Wayland support.

## Targets

| Target | Output |
| --- | --- |
| `deimos_core` | static library with the shared model, transport and supervisors |
| `deimos_daemon` | static library with the engine, config store and REST API |
| `deimosd` | daemon executable |
| `deimos` | single client executable: GUI and CLI |
| `deimos_tests` | Catch2 test suite |
| `deimos_fake_zapret` | test helper that pretends to be winws/nfqws |

## Options

| Option | Default | Meaning |
| --- | --- | --- |
| `DEIMOS_BUILD_DAEMON` | ON | build `deimosd` |
| `DEIMOS_BUILD_CLI` | ON | include the command line client in `deimos` |
| `DEIMOS_BUILD_UI` | ON | include the GUI in `deimos` |
| `DEIMOS_BUILD_TESTS` | ON for a top-level build | build and register tests |
| `DEIMOS_SERVER_ONLY` | OFF | force daemon-only: no UI, no CLI, no tests, static runtime |
| `DEIMOS_STATIC_RUNTIME` | ON with MSVC | link the C++ runtime statically |
| `DEIMOS_FETCH_DEPS` | ON | download dependencies when they are not installed |
| `DEIMOS_ENABLE_TLS` | OFF | compile HTTPS support into daemon and client |

Examples:

```bash
# headless daemon for a router
cmake -S . -B build/daemon -G Ninja -DDEIMOS_SERVER_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/daemon

# client without the GUI, e.g. in a container
cmake -S . -B build/cli -G Ninja -DDEIMOS_BUILD_UI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/cli

# GUI only
cmake -S . -B build/gui -G Ninja -DDEIMOS_BUILD_CLI=OFF
```

## Tests

```bash
ctest --preset linux-debug
./out/build/linux-debug/bin/deimos_tests "[zapret]"
```

On Windows: `ctest --preset x64-release` and
`.\out\build\x64-release\bin\deimos_tests.exe "[zapret]"`.

The suite covers configuration round-trips and validation, zapret command generation for
all three tools, process supervision (including the missing-binary path) and the HTTP API
against an in-process daemon on an ephemeral port.

## Packaging

`cpack` is configured for ZIP and NSIS on Windows and TGZ/DEB elsewhere. Components are
`daemon`, `ui`, `cli` and `runtime`, so a server package can be built without any client
artefacts:

```bash
cmake --preset daemon-only
cmake --build --preset daemon-only
cmake --install out/build/daemon-only --prefix /opt/deimos --component daemon

cd out/build/linux-release && cpack -G ZIP -D CPACK_COMPONENTS_ALL=daemon
```

## Formatting

`.clang-format` is Allman style with 4 spaces and 120 columns:

```bash
clang-format -i src/**/*.cpp include/**/*.hpp
```
