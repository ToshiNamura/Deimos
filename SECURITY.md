<p align="center">
  <b>🇬🇧 English</b> &nbsp;·&nbsp; <a href="docs/ru/SECURITY.md">🇷🇺 Русский</a>
</p>

# Security policy

## Supported versions

| Version | Supported |
| --- | --- |
| 0.1.x | yes |

Only the latest release receives fixes.

## Reporting a vulnerability

Use GitHub's private vulnerability reporting: open the **Security** tab of
[ToshiNamura/Deimos](https://github.com/ToshiNamura/Deimos/security) and choose
**Report a vulnerability**. Do not open a public issue.

Please include what you can, without pasting live secrets:

- the version printed by `deimosd --version` and `deimos --version`;
- the platform and how the daemon runs (foreground, Windows service, systemd, launchd,
  container);
- the affected endpoint or command;
- a minimal reproduction and, if you have one, a suggested fix.

Never include bearer tokens, tokens from `<state>/token`, TLS private keys or private host
names. A redacted configuration is enough.

## Scope

In scope:

- authentication and authorisation of the daemon API: token comparison, token storage
  permissions, endpoints reachable without a token;
- remote exposure: enforcement of `allowRemoteClients` and `requireAuthentication`, TLS
  configuration;
- injection into the generated zapret command line (profile fields, host lists, paths,
  `extraArguments`) or into the generated systemd unit and launchd plist;
- process supervision: escaping the Windows job object or the POSIX process group, child
  processes surviving daemon shutdown, privilege handling;
- denial of service through the HTTP server (request size, slow clients, unbounded log
  growth) or through configuration writes.

Out of scope:

- vulnerabilities in zapret, WinDivert or the operating system; report those upstream;
- a daemon deliberately exposed to the internet without TLS termination or under a
  world-readable token;
- issues that require an already compromised host or administrator/root access.

## Hardening

These properties are part of the design and are covered by tests or configuration checks:

- the daemon listens on `127.0.0.1`; any other address requires `allowRemoteClients: true`
  and `requireAuthentication: true`, otherwise the configuration is rejected;
- every endpoint except `/api/v1/health` requires `Authorization: Bearer <token>`, compared
  in constant time;
- `deimosd --generate-token` writes the token with owner-only permissions and prints it
  once;
- TLS is optional (`-DDEIMOS_ENABLE_TLS=ON`); for anything beyond a trusted network,
  terminate TLS in a reverse proxy or use an SSH tunnel, as described in
  [docs/deployment.md](docs/deployment.md).

## Disclosure

Fixes ship with a `CHANGELOG.md` entry. Reports that arrive through private reporting get
a GitHub security advisory, crediting the reporter unless they prefer otherwise.
