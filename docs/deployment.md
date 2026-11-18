# Deployment

The daemon is the only component that has to run on the machine whose traffic is being
modified. Machines without a GUI can build the daemon alone:

```bash
cmake --preset daemon-only
cmake --build --preset daemon-only
```

That produces one `deimosd` binary with a static C++ runtime and no UI, CLI or test
dependencies.

## Prerequisites

| Platform | What the daemon needs |
| --- | --- |
| Windows | zapret with `winws.exe`; WinDivert ships inside zapret. The service must run as `LocalSystem` or an administrator account |
| Linux host | zapret with `nfqws`/`tpws`, the `nfnetlink_queue` module, and `CAP_NET_ADMIN` + `CAP_NET_RAW` |
| Linux router | the same, plus `nfqws` attached to the forward chain. Upload and download are separate traffic paths, so a router usually needs two rounds tuned for both directions |
| macOS | zapret built for the platform; the daemon needs root |

Point the daemon at the installation explicitly when it is not in a standard place:

```json
"zapret": { "installDirectory": "/opt/zapret" }
```

`deimos doctor` (or `GET /api/v1/zapret`) reports what was found and which paths were
searched.

## First run

```bash
deimosd --generate-token              # prints the token once, writes <state>/token
deimosd --config /etc/deimos/deimos.json --validate-config
deimosd --config /etc/deimos/deimos.json --foreground
```

State and configuration live in:

| Platform | Configuration | State, logs, token |
| --- | --- | --- |
| Windows | `%APPDATA%\Deimos\deimos.json` | `%LOCALAPPDATA%\Deimos\` |
| Linux | `~/.config/deimos/deimos.json` | `~/.local/state/deimos/` |
| macOS | `~/Library/Application Support/Deimos/deimos.json` | same directory |

Logs are written to `deimosd.log` in the state directory in addition to the in-memory
history served by the API.

## Windows service

Run an elevated prompt:

```powershell
deimosd --install-service --config C:\ProgramData\Deimos\deimos.json
sc query deimosd
deimosd --uninstall-service
```

The service is registered as `deimosd`, starts automatically and is started immediately by
the installer. It is a normal Windows service: `sc stop deimosd`, `sc start deimosd` and
the Event Log behave as expected.

## systemd

```bash
deimosd --print-unit > /etc/systemd/system/deimosd.service
systemctl daemon-reload
systemctl enable --now deimosd
```

`deimosd --install-service` does the same steps for you when run as root. The generated
unit grants only `CAP_NET_ADMIN` and `CAP_NET_RAW`.

## launchd

```bash
sudo deimosd --print-plist > /Library/LaunchDaemons/com.deimos.daemon.plist
sudo launchctl load -w /Library/LaunchDaemons/com.deimos.daemon.plist
```

## Clients

```bash
deimos --gui                                  # desktop interface
deimos --endpoint http://192.168.1.1:8787 status
deimos --endpoint http://192.168.1.1:8787 start --profile tls-split
```

Saved endpoints are kept in `<config>/cli.json` (CLI) and `<config>/ui.json` (GUI):

```bash
deimos endpoints add router http://192.168.1.1:8787 --token-file /root/router.token
deimos endpoints use router
deimos doctor
```

Token resolution order for both clients:

1. `--token`
2. `$DEIMOS_TOKEN`
3. `--token-file`
4. the token file recorded for the selected endpoint
5. the platform default token path

## Exposing the daemon

The default configuration only listens on loopback. To manage a remote daemon, either
tunnel the port or enable remote access explicitly:

```json
"daemon": {
  "listenAddress": "0.0.0.0",
  "port": 8787,
  "allowRemoteClients": true,
  "requireAuthentication": true
}
```

The daemon refuses to start if `allowRemoteClients` is false for a non-loopback address,
or if remote access is combined with disabled authentication.

For anything beyond a trusted LAN, terminate TLS in a reverse proxy (Caddy, nginx, stunnel)
or use an SSH tunnel and keep the daemon on loopback:

```bash
ssh -L 8787:127.0.0.1:8787 root@router
deimos status
```

Compiling with `-DDEIMOS_ENABLE_TLS=ON` and setting `enableTls`, `tlsCertificateFile` and
`tlsPrivateKeyFile` lets the daemon serve HTTPS directly.

## Operations

| Task | Command |
| --- | --- |
| Check connectivity, auth and zapret | `deimos doctor` |
| Inspect what will be executed | `deimos profiles preview <id>` |
| Follow the daemon log | `deimos logs --follow` |
| Restart the engine after editing rules | `deimos restart` |
| Change the active profile | `deimos profiles activate <id>` |
| Rotate the token | `deimosd --generate-token` and restart the daemon |

Restart the daemon after rotating the token, and update the clients afterwards.

## Troubleshooting

| Symptom | Likely cause |
| --- | --- |
| `zapret: not detected` | `installDirectory` is wrong, or the tree does not contain `winws`/`nfqws`; the daemon log lists every searched path |
| `start failed: zapret 'winws' is not available` | the profile pins a tool that is not installed on that host; use `auto` |
| Engine goes to `faulted` right after start | zapret exited immediately; the log contains its stderr lines under `zapret/<profile>` |
| `401` from a client | the token file differs between the client and the daemon, or the service user cannot read it |
| Traffic unaffected although the engine runs | zapret needs the right filter and interface on routers, and administrator rights on Windows |
