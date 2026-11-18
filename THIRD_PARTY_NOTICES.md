# Third-party notices

Deimos itself is distributed under the MIT license (see [LICENSE](LICENSE)). Release
binaries link the libraries below. Their sources and license texts are available from the
upstream projects.

| Library | Version | License | Used by |
| --- | --- | --- | --- |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | MIT | configuration and API serialisation |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | 0.66.0 | MIT | daemon HTTP server, client transport |
| [Catch2](https://github.com/catchorg/Catch2) | 3.16.1 | BSL-1.0 | test suite (never shipped in releases) |
| [GLFW](https://github.com/glfw/glfw) | 3.4 | zlib/libpng | desktop client window and input |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.92.9b | MIT | desktop client interface |

Versions are pinned in [cmake/DeimosDependencies.cmake](cmake/DeimosDependencies.cmake).

## zapret

[zapret](https://github.com/bol-van/zapret) and
[zapret2](https://github.com/bol-van/zapret2) are separate projects with their own
licenses. Deimos never bundles, links or redistributes them: it launches the binaries
installed on the target machine and passes them command line arguments. Users are
responsible for obtaining and licensing zapret themselves.
