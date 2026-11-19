<p align="center">
  <a href="../daemon-api.md">🇬🇧 English</a> &nbsp;·&nbsp; <b>🇷🇺 Русский</b>
</p>

# API демона

Все эндпоинты находятся под `/api/v1`. Если не указано иное, запросы и ответы — JSON.
`GET /api/v1/health` — единственный эндпоинт без аутентификации.

```
Authorization: Bearer <token>
```

## Ошибки

```json
{
  "error": {
    "code": "not_found",
    "message": "profile not found",
    "detail": "youtube-quic"
  }
}
```

| `code` | HTTP-статус |
| --- | --- |
| `invalid_argument`, `configuration_error` | 400 |
| `unauthorized` | 401 |
| `not_found` | 404 |
| `already_exists`, `conflict` | 409 |
| `not_supported` | 422 |
| `io_error`, `network_error`, `internal_error` | 500 |
| `unavailable` | 503 |

Запись профиля, не прошедшая валидацию, возвращает 400 с обычным объектом ошибки и
массивом `diagnostics`, чтобы клиент мог показать, какое именно поле неверно.

Поля `message`, `detail` и сообщения диагностики следуют языку демона (`daemon.language`,
`--lang`); поля `code` и `code` в диагностике никогда не меняются.
См. [Локализация](./localization.md).

## Проверка доступности

`GET /api/v1/health` (также `GET /health`), токен не требуется.

```json
{ "status": "ok", "version": "0.1.0", "uptimeSeconds": 42 }
```

## Статус

`GET /api/v1/status`

```json
{
  "version": "0.1.0",
  "platform": "windows",
  "architecture": "x86_64",
  "hostname": "router",
  "listenAddress": "127.0.0.1",
  "port": 8787,
  "uptimeSeconds": 120,
  "authenticationRequired": true,
  "zapret": {
    "available": true,
    "baseDirectory": "C:/zapret",
    "tools": ["winws"]
  },
  "engine": {
    "state": "running",
    "activeProfileId": "tls-split",
    "uptimeSeconds": 30,
    "restartCount": 0,
    "launches": [
      {
        "label": "tls-split",
        "command": "C:/zapret/binaries/windows-x86_64/winws.exe --wf-tcp=443 ...",
        "pid": 4242
      }
    ],
    "lastError": "",
    "diagnostics": []
  }
}
```

`engine.state` принимает одно из значений: `stopped`, `starting`, `running`, `stopping`,
`faulted`.

## Управление движком

| Метод | Путь | Тело | Результат |
| --- | --- | --- | --- |
| POST | `/api/v1/engine/start` | `{"profileId": "tls-split"}` (необязательно) | снимок состояния движка |
| POST | `/api/v1/engine/stop` | - | снимок состояния движка |
| POST | `/api/v1/engine/restart` | `{"profileId": "..."}` (необязательно) | снимок состояния движка |

Без `profileId` демон использует активный профиль, а затем профиль, который работал
последним. Запуск уже работающего движка возвращает 409.

```bash
curl -X POST -H "Authorization: Bearer $TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"profileId":"tls-split"}' \
     http://127.0.0.1:8787/api/v1/engine/start
```

## Профили

| Метод | Путь | Описание |
| --- | --- | --- |
| GET | `/api/v1/profiles` | `{"profiles": [...], "activeProfileId": "tls-split"}` |
| GET | `/api/v1/profiles/{id}` | полный объект профиля |
| PUT | `/api/v1/profiles/{id}` | создание (201) или замена (200) |
| DELETE | `/api/v1/profiles/{id}` | удаление |
| POST | `/api/v1/profiles/{id}/activate` | сделать активным и перезапустить движок, если он работает |
| GET | `/api/v1/profiles/{id}/preview` | `{"commands": [...], "diagnostics": [...]}` |

`PUT` игнорирует `id` в теле и использует путь, поэтому клиент может переименовать
профиль, записав новый id и удалив старый.

### Документ профиля

```json
{
  "id": "tls-split",
  "name": "TLS split",
  "description": "Splits the ClientHello",
  "notes": "",
  "tool": "auto",
  "nfqueue": 200,
  "winDivertTcpPorts": [],
  "winDivertUdpPorts": [],
  "rounds": [
    {
      "filter": { "tcp": [443], "udp": [], "l7": ["tls"] },
      "modes": ["split2"],
      "splitPos": 1,
      "splitSeqOverlap": null,
      "fooling": [],
      "ttl": null,
      "autoTtl": 2,
      "repeats": null,
      "windowSize": null,
      "fakePayload": null,
      "hostlists": [],
      "hostlistExcludes": [],
      "ipsets": []
    }
  ],
  "hostlists": [],
  "hostlistExcludes": [],
  "ipsets": [],
  "extraArguments": [],
  "enabled": true
}
```

- `tool`: `auto`, `winws`, `nfqws`, `tpws`.
- `rounds[].modes`: `split`, `split2`, `disorder`, `disorder2`, `fake`, `fakedsplit`,
  `fakeddisorder`, `hostfakesplit`.
- `rounds[].fooling`: `md5sig`, `badseq`, `badsum`, `datanoack`, `hopbyhop`, `hopbyhop2`.
- `rounds[].filter.l7`: `tls`, `http`, `quic`, `unknown`.
- `fakePayload`: `{"kind": "file", "value": "files/fake/..."}` или
  `{"kind": "hex", "value": "0102aabb"}`.

О том, как они отображаются на аргументы zapret, см.
[Стратегии и отображение на zapret](./strategies.md).

## Конфигурация

| Метод | Путь | Описание |
| --- | --- | --- |
| GET | `/api/v1/config` | полный документ конфигурации |
| PUT | `/api/v1/config` | проверить, сохранить и применить |

```json
{
  "schemaVersion": 1,
  "daemon": {
    "listenAddress": "127.0.0.1",
    "port": 8787,
    "allowRemoteClients": false,
    "requireAuthentication": true,
    "tokenFile": "",
    "logLevel": "info",
    "logHistorySize": 2000,
    "autoStartEngine": false,
    "activeProfileId": "tls-split",
    "enableTls": false,
    "tlsCertificateFile": "",
    "tlsPrivateKeyFile": ""
  },
  "engine": {
    "maxRestarts": 5,
    "restartBackoffMs": 1500,
    "stopTimeoutMs": 5000,
    "restartOnCrash": true
  },
  "zapret": {
    "installDirectory": "",
    "winwsPath": "",
    "nfqwsPath": "",
    "tpwsPath": "",
    "workingDirectory": ""
  },
  "profiles": []
}
```

Пустой `zapret.installDirectory` означает «определять автоматически» (см.
[Развёртывание](./deployment.md)). Применение конфигурации при работающем движке
останавливает и перезапускает его с активным профилем.

## Журналы

`GET /api/v1/logs?limit=200&level=info&after=1234`

- `limit` — от 1 до 2000, по умолчанию 200.
- `level` — минимальный уровень: `trace`, `debug`, `info`, `warn`, `error`.
- `after` возвращает только записи с большим номером последовательности; для слежения в
  стиле `tail` опрашивайте эндпоинт с полученным `lastSequence`.

```json
{
  "entries": [
    {
      "sequence": 42,
      "time": "2026-11-18T13:41:44.409Z",
      "level": "info",
      "source": "zapret/tls-split",
      "message": "arg1=--wf-tcp=443"
    }
  ],
  "lastSequence": 42
}
```

## Инструментарий zapret

`GET /api/v1/zapret`

```json
{
  "available": true,
  "baseDirectory": "C:/zapret",
  "filesDirectory": "C:/zapret/files",
  "listsDirectory": "C:/zapret/lists",
  "tools": ["winws"],
  "diagnostics": [{ "severity": "info", "code": "zapret.tool.found", "message": "winws: ...", "path": "" }]
}
```

Этот эндпоинт повторно выполняет обнаружение, поэтому он подходит для проверки в
стиле «доктора».

---

Перевод файла [daemon-api.md](../daemon-api.md). Синхронизировано с коммитом c679559.
