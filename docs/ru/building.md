<p align="center">
  <a href="../building.md">🇬🇧 English</a> &nbsp;·&nbsp; <b>🇷🇺 Русский</b>
</p>

# Сборка

## Требования

- CMake 3.25 или новее
- Компилятор C++20: MSVC 19.4x (Visual Studio 2022/2026), GCC 12+, Clang 15+
- Доступ к сети при первой конфигурации либо зависимости, установленные в системе
  (nlohmann/json, cpp-httplib, GLFW, Dear ImGui, Catch2)

Для GUI нужен драйвер с поддержкой OpenGL 3.2. Всё остальное работает без интерфейса.

## Пресеты

Пресеты используют Ninja с компилятором платформы, поэтому им нужна оболочка разработчика
(`vcvars64.bat` в Windows, обычная оболочка с компилятором в Linux и macOS):

| Пресет | Что настраивает |
| --- | --- |
| `x64-debug`, `x64-release` | Windows, Ninja с `cl.exe`, Debug или Release |
| `linux-debug`, `linux-release` | Linux, Ninja, Debug или Release |
| `macos-debug`, `macos-release` | macOS, Ninja, Debug или Release |
| `daemon-only` | только `deimosd`, статическая среда выполнения, без UI/CLI/тестов, любая платформа |

Пресеты платформ привязаны к хост-системе, поэтому `x64-release` настраивается только в
Windows, а `linux-debug` — только в Linux. Сборки попадают в `out/build/<preset>`, а
установка — в `out/install/<preset>`:

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

В Linux для GUI нужны заголовочные файлы разработки X11; CI передаёт GLFW флаг
`-DGLFW_BUILD_WAYLAND=OFF`, чтобы на этапе конфигурации не требовался `wayland-scanner`.
Уберите флаг, если инструментарий установлен и нужна нативная поддержка Wayland.

## Цели сборки

| Цель | Выходной файл |
| --- | --- |
| `deimos_core` | статическая библиотека с общей моделью, транспортом и супервизорами |
| `deimos_daemon` | статическая библиотека с движком, хранилищем конфигурации и REST API |
| `deimosd` | исполняемый файл демона |
| `deimos` | единственный исполняемый файл клиента: GUI и CLI |
| `deimos_tests` | набор тестов Catch2 |
| `deimos_fake_zapret` | тестовый помощник, который притворяется winws/nfqws |

## Опции

| Опция | По умолчанию | Значение |
| --- | --- | --- |
| `DEIMOS_BUILD_DAEMON` | ON | собирать `deimosd` |
| `DEIMOS_BUILD_CLI` | ON | включить клиент командной строки в `deimos` |
| `DEIMOS_BUILD_UI` | ON | включить GUI в `deimos` |
| `DEIMOS_BUILD_TESTS` | ON для сборки верхнего уровня | собирать и регистрировать тесты |
| `DEIMOS_SERVER_ONLY` | OFF | принудительно только демон: без UI, CLI и тестов, статическая среда выполнения |
| `DEIMOS_STATIC_RUNTIME` | ON с MSVC | линковать среду выполнения C++ статически |
| `DEIMOS_FETCH_DEPS` | ON | скачивать зависимости, если они не установлены |
| `DEIMOS_ENABLE_TLS` | OFF | скомпилировать поддержку HTTPS в демон и клиент |

Примеры:

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

## Тесты

```bash
ctest --preset linux-debug
./out/build/linux-debug/bin/deimos_tests "[zapret]"
```

В Windows: `ctest --preset x64-release` и
`.\out\build\x64-release\bin\deimos_tests.exe "[zapret]"`.

Набор тестов покрывает круговые преобразования конфигурации и валидацию, генерацию команд
zapret для всех трёх инструментов, надзор за процессами (включая случай отсутствующего
бинарника) и HTTP API против демона внутри процесса на эфемерном порту.

## Упаковка

`cpack` настроен на ZIP и NSIS в Windows и на TGZ/DEB в остальных системах. Компоненты —
`daemon`, `ui`, `cli` и `runtime`, поэтому серверный пакет можно собрать вообще без клиентских
артефактов:

```bash
cmake --preset daemon-only
cmake --build --preset daemon-only
cmake --install out/build/daemon-only --prefix /opt/deimos --component daemon

cd out/build/linux-release && cpack -G ZIP -D CPACK_COMPONENTS_ALL=daemon
```

## Форматирование

`.clang-format` — стиль Allman с 4 пробелами и 120 столбцами:

```bash
clang-format -i src/**/*.cpp include/**/*.hpp
```

---

Перевод файла [building.md](../building.md). Синхронизировано с коммитом c679559.
