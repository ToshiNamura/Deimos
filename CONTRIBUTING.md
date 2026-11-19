<p align="center">
  <b>🇬🇧 English</b> &nbsp;·&nbsp; <a href="docs/ru/CONTRIBUTING.md">🇷🇺 Русский</a>
</p>

# Contributing to Deimos

Thanks for taking the time to improve Deimos. This page describes how the project is
built, tested and styled, so a change can land without back-and-forth.

## Before you start

- **Bug reports** are welcome as issues. Include the reproduction, the daemon log and your
  platform, as the issue form asks.
- **Larger features** (a new profile field, a configuration change, an API change) are
  worth an issue first: they touch the schema, the API documentation and the client at the
  same time.
- **Security issues** go through [SECURITY.md](SECURITY.md), never a public issue.

## Prerequisites

| Requirement | Notes |
| --- | --- |
| CMake 3.25 or newer | the Ninja presets also need Ninja and a developer shell |
| C++20 compiler | MSVC 19.4x (Visual Studio 2022/2026), GCC 12+, Clang 15+ |
| GUI dependencies on Linux | `libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`, or configure with `-DDEIMOS_BUILD_UI=OFF` |
| zapret | only to drive real traffic; the test suite uses `deimos_fake_zapret` |

nlohmann/json, cpp-httplib, Catch2, GLFW and Dear ImGui are downloaded with FetchContent on
the first configure. Pass `-DDEIMOS_FETCH_DEPS=OFF` to use system packages instead.

## Build and test

The Ninja presets expect a developer shell (`vcvars64.bat` on Windows, a normal shell with
a compiler on Linux and macOS). There is one preset pair per platform, and they only
configure on their own host:

```bash
cmake --preset linux-debug      # or x64-debug on Windows, macos-debug on macOS
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Builds land in `out/build/<preset>` and installs in `out/install/<preset>`.

`daemon-only` builds a single `deimosd` without the UI, the CLI or the tests:

```bash
cmake --preset daemon-only
cmake --build --preset daemon-only
```

Running one area of the suite uses Catch2 tags:

```bash
./out/build/linux-debug/bin/deimos_tests "[zapret]"
```

The available tags are `[config]`, `[protocol]`, `[zapret]`, `[process]` and `[api]`.

## Tests

- Add tests for the behaviour you add or fix in the existing file for that area:
  `tests/test_config.cpp`, `test_protocol.cpp`, `test_command_builder.cpp`,
  `test_process.cpp` or `test_api.cpp`.
- Test names read `subject_scenario_expected`, for example
  `buildLaunches_TpwsMapsSplitAndDisorder`. Prefer tests that fail before the change and
  pass after it.
- `tests/fake_zapret.cpp` stands in for `winws`/`nfqws`; extend it when you need a new
  process behaviour instead of mocking the supervisor.

## Style

Formatting is defined by [.clang-format](.clang-format) and [.editorconfig](.editorconfig):
Allman braces, 4 spaces, 120 columns, LF, UTF-8, no trailing whitespace.

- `PascalCase` for types, `camelCase` for functions, methods, locals and fields,
  `UPPER_SNAKE_CASE` for constants.
- Acronyms are words: `userId`, `httpClient`, never `userID` or `HTTPClient`. Booleans read
  as predicates: `isReady`, `hasPermission`.
- No anonymous namespaces; use `static` or a named `detail` namespace.
- Run `clang-format -i path/to/file.cpp` on the files you touch. The tree has not been
  formatted as a whole with the current profile yet, so do not mix a repository-wide
  reformat into a functional change.
- Keep the build warning-free: MSVC runs `/W4`, GCC and Clang run `-Wall -Wextra -Wpedantic
  -Wshadow -Wconversion -Wsign-conversion`, and CI builds the Linux job with
  `-DDEIMOS_WARNINGS_AS_ERRORS=ON`.

## Commits and pull requests

- One logical change per commit, imperative subject ("Add tpws split fallback"), and a body
  that explains why the change is needed.
- Update [CHANGELOG.md](CHANGELOG.md) under `[Unreleased]` when the change is user visible.
- Update `docs/` when an endpoint, option, profile field or deployment step changes.
- Open the pull request with the template: what changed, which issue it closes, and the
  exact commands you ran to verify it.
- Both CI jobs, Linux and Windows, must pass.

## Project layout

| Path | Contents |
| --- | --- |
| `include/deimos/` | public headers of `deimos_core` |
| `src/core/` | model, transport, zapret mapping, process supervision |
| `src/daemon/` | engine, configuration store, REST API, service integration |
| `src/client/` | `deimos` entry point, command line client and ImGui interface |
| `tests/` | Catch2 suite and the fake zapret helper |
| `cmake/` | options, dependencies, compiler flags, packaging, configure summary |
| `docs/` | architecture, daemon API, strategies, building, deployment |

## Licence

Contributions are accepted under the MIT licence in [LICENSE](LICENSE).
