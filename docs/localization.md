<p align="center">
  <b>🇬🇧 English</b> &nbsp;·&nbsp; <a href="ru/localization.md">🇷🇺 Русский</a>
</p>

# Localisation

Deimos ships English and Russian in the same binary. Nothing is downloaded and nothing is
read from the disk at start-up: the catalogue is compiled in.

## Choosing a language

| Surface | How the language is chosen |
| --- | --- |
| `deimos` (GUI and CLI) | `--lang auto\|en\|ru`, otherwise `"language"` in `ui.json` when the desktop interface is used, otherwise the system UI language |
| `deimosd` | `--lang auto\|en\|ru`, otherwise `daemon.language`, otherwise the system UI language |

`auto` means "follow the operating system". Detection order:

1. `DEIMOS_LANG` - `en`, `ru`, or any BCP 47 tag such as `ru-RU`;
2. `LC_ALL`, `LC_MESSAGES`, `LANG` on POSIX;
3. `GetUserDefaultUILanguage()` on Windows;
4. English.

Anything the catalogue does not recognise falls back to English. `--lang` on the command
line always wins for that process and is not persisted; the persisted setting is the
language selector in the sidebar of the desktop interface. Command line runs without a
selector therefore follow `--lang` and the system language, exactly like the daemon.

The interface can switch language at runtime - the selector in the sidebar footer applies
immediately and is stored in `ui.json`:

```json
{ "language": "ru" }
```

## What is translated

- Every string of the desktop interface, the CLI, the command line help and the shell
  diagnostics.
- Validation diagnostics, both from the daemon (`/api/v1/...` responses, logs) and from the
  client when it validates a profile locally.
- Error payloads returned by the daemon, and the client-side network errors.
- The daemon's own log lines: start-up, API binding, configuration reloads, engine state
  changes.

Deliberately **not** translated:

- Anything produced by zapret itself. The daemon forwards zapret's stdout and stderr
  verbatim, because those lines are diagnostic output of a third-party tool and are matched
  by users against upstream documentation.
- Machine readable values: error codes (`"code": "profile.invalid"`), JSON field names, HTTP
  methods, profile ids and tool names (`winws`, `nfqws`, `tpws`).
- The `api.error.*` `code` field. Only the human readable `message` and `hint` follow the
  daemon language.

The daemon answers in **its** language, not the client's: a Russian client talking to an
English daemon receives English diagnostics. This is intentional - the daemon may serve
several clients, and its log has exactly one language. Set `daemon.language` to pin it.

## Adding a string

1. Add the key to `src/core/support/i18n_catalog.inc` in **both** `ENGLISH_ENTRIES` and
   `RUSSIAN_ENTRIES`, in the same position and order.
2. Use positional placeholders `{0}`, `{1}`, ... so a translation can reorder them.
3. Read the string with `deimos::i18n::tr("key")` or
   `deimos::i18n::tr("key", {arg0, arg1})`.

Key groups:

| Prefix | Contents |
| --- | --- |
| `diag.` | Validation diagnostics. The key is `"diag." + diagnostic code`, so a message can always be traced back to a stable code. |
| `err.` | `Error` messages carried by `Status` and the transport layer. |
| `api.error.` | Error payloads of the REST API. |
| `log.` | Daemon log lines and forwarded engine events. |
| `ui.`, `cli.`, `daemon.usage.` | Interface, command line and help text. |
| `state.`, `tool.`, `level.`, `severity.`, `time.` | Enum labels and units, returned by `deimos::labels`. |

Diagnostics are emitted through `deimos::model::errorAt`, `warningAt` and `infoAt`:

```cpp
diagnostics.push_back(errorAt("config.port.out_of_range", "daemon.port", i18n::tr(...)));
```

`tests/test_i18n.cpp` fails when the two catalogues diverge, when a key is duplicated, or
when a translation returns the raw key.

## Adding a language

1. Add the value to `deimos::i18n::Language` and to `parseLanguage`, `code` and `resolve` in
   `src/core/support/i18n.cpp`.
2. Add a `LANGUAGE_ENTRIES` array to `src/core/support/i18n_catalog.inc` and register it in
   the lookup table in `i18n.cpp`.
3. Add the language to the interface selector (`languageCodes` and its labels in
   `App::renderSidebar`) and to the `daemon.language` validation list in
   `src/core/model/config.cpp`.
4. Extend `tests/test_i18n.cpp` to check the new catalog for parity and empty values.

The `en` and `ru` catalogues currently contain about 390 keys each.
