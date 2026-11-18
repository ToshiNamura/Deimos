# Daemon API

All endpoints live under `/api/v1`. Requests and responses are JSON unless stated
otherwise. `GET /api/v1/health` is the only unauthenticated endpoint.

```
Authorization: Bearer <token>
```

## Errors

```json
{
  "error": {
    "code": "not_found",
    "message": "profile not found",
    "detail": "youtube-quic"
  }
}
```

| `code` | HTTP status |
| --- | --- |
| `invalid_argument`, `configuration_error` | 400 |
| `unauthorized` | 401 |
| `not_found` | 404 |
| `already_exists`, `conflict` | 409 |
| `not_supported` | 422 |
| `io_error`, `network_error`, `internal_error` | 500 |
| `unavailable` | 503 |

Profile writes that fail validation answer 400 with the usual error object plus a
`diagnostics` array, so a client can show exactly which field is wrong.

## Health

`GET /api/v1/health` (also `GET /health`), no token required.

```json
{ "status": "ok", "version": "0.1.0", "uptimeSeconds": 42 }
```

## Status

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

`engine.state` is one of `stopped`, `starting`, `running`, `stopping`, `faulted`.

## Engine control

| Method | Path | Body | Result |
| --- | --- | --- | --- |
| POST | `/api/v1/engine/start` | `{"profileId": "tls-split"}` (optional) | engine snapshot |
| POST | `/api/v1/engine/stop` | - | engine snapshot |
| POST | `/api/v1/engine/restart` | `{"profileId": "..."}` (optional) | engine snapshot |

Without `profileId` the daemon uses its active profile, then the profile that was last
running. Starting an already running engine answers 409.

```bash
curl -X POST -H "Authorization: Bearer $TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"profileId":"tls-split"}' \
     http://127.0.0.1:8787/api/v1/engine/start
```

## Profiles

| Method | Path | Description |
| --- | --- | --- |
| GET | `/api/v1/profiles` | `{"profiles": [...], "activeProfileId": "tls-split"}` |
| GET | `/api/v1/profiles/{id}` | full profile object |
| PUT | `/api/v1/profiles/{id}` | create (201) or replace (200) |
| DELETE | `/api/v1/profiles/{id}` | delete |
| POST | `/api/v1/profiles/{id}/activate` | set active, restart the engine if running |
| GET | `/api/v1/profiles/{id}/preview` | `{"commands": [...], "diagnostics": [...]}` |

`PUT` ignores the `id` inside the body and uses the path, so a client can rename by
writing a new id and deleting the old one.

### Profile document

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
- `fakePayload`: `{"kind": "file", "value": "files/fake/..."}` or
  `{"kind": "hex", "value": "0102aabb"}`.

See [strategies.md](strategies.md) for how these map onto zapret arguments.

## Configuration

| Method | Path | Description |
| --- | --- | --- |
| GET | `/api/v1/config` | full configuration document |
| PUT | `/api/v1/config` | validate, persist and apply |

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

Empty `zapret.installDirectory` means "detect automatically" (see
[deployment.md](deployment.md)). Applying a configuration while the engine runs stops
and restarts it with the active profile.

## Logs

`GET /api/v1/logs?limit=200&level=info&after=1234`

- `limit` 1..2000, default 200.
- `level` is the minimum level: `trace`, `debug`, `info`, `warn`, `error`.
- `after` returns only entries with a higher sequence number; poll with the returned
  `lastSequence` for tail-style streaming.

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

## zapret toolchain

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

This endpoint re-runs detection, so it is the right call for a "doctor" check.
