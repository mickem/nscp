# Upgrading

What to do when upgrading NSClient++, newest release first. Read the entries
between the version you are on and the version you are moving to. Most upgrades
are in place — defaults are preserved and the default install is usually
unaffected.

Every release is split by how much it asks of you, so you can stop reading at
the point where a section stops applying:

| Section | What it means |
|---|---|
| **Requires action** | An existing install needs a change from you — a setting to add, a threshold, client or alert to review. Every item names its condition up front, so skip the ones that do not apply. |
| **Changes behaviour (no action)** | Something observable changed, but the upgrade is in place. Read it if output, perfdata or alerting looks different afterwards. |
| **New and opt-in (no action)** | Additions that stay off until you turn them on. Nothing breaks if you skip the section entirely. |

Each item appears **once**, under the release that actually introduced it —
including the releases marked as pre-releases, since those are installed too. A
section is omitted when a release has nothing under it, and a release with no
upgrade-relevant change is not listed at all. **Skipping several versions?**
Read every *Requires action* section between your version and your target
first — together they are the complete list of things that will not sort
themselves out. Each item starts with the module, setting or command it
concerns, so searching this page for the name of something you have configured
finds everything that touches it.

Items marked 🔒 are security-relevant; where a notice exists, the item links to
it on the [Security notices](../security/notices.md) page. Full per-release
detail lives in each [GitHub release](https://github.com/mickem/nscp/releases).

---

## 0.16.2 (unreleased)

### Requires action

- 🔒 **WEB `legacy` permission** — if a role in your configuration grants
  `legacy`, startup now logs a `SECURITY` warning (as do `nscp web add-role` /
  `add-user`). That grant unlocks the deprecated `/query.pb` and
  `/query/{name}` endpoints, so a token holding it can run any registered check
  or command: keep it only for trusted systems that cannot use
  `/api/v2/queries`, and drop it elsewhere. Existing installs are otherwise
  unaffected — the role stays and keeps working; fresh installs no longer seed
  it. See [Security notices](../security/notices.md).

### Changes behaviour (no action)

- 🔒 **Settings reads redact sensitive values.** `GET /api/v2/settings/...`,
  `/descriptions` and `nscp settings --list` / `--show` return `***` for keys
  registered sensitive, matching the `diff` endpoint. Tooling that read such a
  value back out now receives `***`. See
  [Security notices](../security/notices.md).

## 0.16.1

### Requires action

- **`${hostname}` in settings URLs is now expanded.** It used to be left in
  place as literal text; it is now expanded everywhere `expand_hostname` is
  used, including the submit clients' `hostname`. Check any configuration that
  relied on the literal.
- **Building from source** — the HTML docs on non-Windows now need
  `-DNSCP_BUILD_DOCS_HTML=ON` (or the `build_docs_html` target). Packagers
  cross-building for another distribution should set `-DCONFIG_CA_PATH=`.

### Changes behaviour (no action)

- **`${ca-path}` resolves on its own on RHEL/SUSE**, so workaround `ca=`
  arguments can be dropped. Naming a bundle explicitly still works.
- **Settings URLs with a query string now send it.** A server that relied on
  receiving the bare path will see the parameters. The offline-boot cache file
  is migrated to the query-aware name once, on first start.

### New and opt-in (no action)

- **`check_logfile` `bookmark` / `max-lines`** — a check with neither reads the
  whole file exactly as before and never touches a stored position. Adopting
  `bookmark` is a trade-off: a line is consumed when the check runs, not when
  its result is submitted, and positions are saved on clean shutdown, so a crash
  re-reports the backlog. Prefer an explicit bookmark name for a check whose
  filter changes often — an automatic name covers the expressions, so editing
  the filter starts a new position.
- **`run on startup`** is off by default. Turning it on for the `default`
  schedule enables it fleet-wide; use `startup window` to spread the resulting
  submissions.

## 0.16.0

### Requires action

- **Filter expressions testing the old `-1` sentinels.** A keyword that has no
  value no longer parks `-1` in itself: it reports *no value*, and every numeric
  comparison against it is false — including `=`, `!=`, `in` and `not in`, the
  way SQL treats NULL. Rewrite such expressions as a presence test on the string
  form:

  | Command | Keywords | Presence test |
  |---|---|---|
  | `check_ping` | `jitter`, `ttl` | `jitter = 'unknown'` |
  | `check_ntp_offset` | `jitter` | `jitter = 'unknown'` |
  | `check_tcp`, `check_http` | `ssl_expiry_days` | `ssl_expiry_days = 'no certificate'` |
  | `check_disk_health` | `total`, `free`, `used`, `user_free`, `free_pct`, `used_pct` | `free_pct = 'no space data'` |

  Two thresholds get better as a result: `critical=ssl_expiry_days < 30` is now
  safe on a plain connection, and `free_pct < 10` no longer fires on
  `check_disk_health` rows that have no filesystem behind them. Sentinels
  elsewhere are untouched — `check_process`'s `uid` still reports `-1` on
  synthetic rows.
- **`check_docker` `host=` only accepts a local endpoint** — a named pipe or an
  absolute socket path. A configuration pointing at a remote or UNC endpoint is
  refused.
- **CheckDocker requires Docker API 1.41 or newer** (the pinned `/v1.40` prefix
  is gone). Any daemon from Docker 20.10 onwards qualifies.

### Changes behaviour (no action)

- **`check_disk_health` and `check_disk_io` perfdata series are renamed.**
  `free_pct`, `queue_length` and `percent_disk_time` carry their own label
  suffixes instead of sharing the bare drive label, so Graphite and InfluxDB see
  new series — the old collapsed series held the wrong metric anyway. Icinga
  shows two correctly named entries where it showed two under one name.
- **Perfdata is omitted for values that were not measured** rather than plotted
  as `-1`, so those series appear and disappear instead of carrying a fake
  floor.

### New and opt-in (no action)

- **CheckMySQL** — a new module for MySQL, MariaDB and Percona servers, not
  loaded by default. It is built wherever MariaDB Connector/C is available; the
  Windows installer ships it as its own feature (`MySQLPlugin`, which bundles
  the Connector/C library), installed by default and removable from the feature
  tree or with `REMOVE=MySQLPlugin`.

## 0.15.0

### Requires action

- **`check_process delta=true` requires the `process cpu` collector setting.**
  With the collector off the check returns UNKNOWN naming the setting, instead
  of sleeping one second inside the check. With it on, memory and handle fields
  report absolute values in delta mode rather than 1-second differences.
  Installs that do not use `delta=true` are unaffected.

### New and opt-in (no action)

- **CheckMSSQL** — a new optional module, not loaded by default. Enable it and
  see the *Monitoring a SQL Server host* scenario in the docs.

## 0.14.1

### Requires action

- **CheckNet emits perfdata by default.** Network checks produce performance
  data without an explicit `perf` syntax: if you were adding it manually, check
  you are not now emitting it twice. Graphs that showed nothing start
  populating.
- **Licence change** — NSClient++ is distributed as **Apache-2.0 OR
  GPL-2.0-only**. A clarification/relicensing with no code or runtime change;
  review it only if your organisation tracks the licence of bundled software.

### Changes behaviour (no action)

- **Boolean check arguments** (`option=true` / `option=false`) now work from the
  CLI as well as over REST. Bare-flag usage is unchanged.

### New and opt-in (no action)

- **CheckSecurity** — a new module, not loaded by default. Enable it before
  using its checks (`nscp settings --active-module CheckSecurity`). Its
  Windows-only checks return UNKNOWN on other platforms rather than erroring.

## 0.14.0

### Requires action

- **Linux `check_service` targets systemd.** Match units by unit name
  (e.g. `service=ssh`) and use `state`, `active`, `sub_state` and `preset` in
  thresholds; the default expression keeps *disabled* units OK. Rewrite rules
  written against the old behaviour.
- **Linux `check_process` reports delta CPU** — usage between samples rather
  than lifetime CPU. Review CPU thresholds that assumed the old semantics.

### Changes behaviour (no action)

- **Linux disk I/O needs one collector sample.** The first `check_disk_io` /
  `check_disk_health` query immediately after startup may return UNKNOWN
  ("collector still initializing") — normal in one-shot testing, invisible with
  a running service.
- **`check_tcp` / `check_http` take a boolean `ssl`.** Enable TLS with
  `ssl=true`; to verify certificates set `verify=peer` and provide a `ca=`
  bundle. The default remains `verify=none`.

## 0.13.0

Linux users and anyone running the web server in cleartext should read this
section; a normal Windows MSI upgrade is unaffected.

### Requires action

- 🔒 **The web server refuses to run unencrypted.** Provide a certificate
  (`certificate = …`), or opt in explicitly if you intentionally run cleartext
  behind a TLS-terminating proxy or on an isolated network:

  ```ini
  [/settings/WEB/server]
  allow insecure = true
  ```

  With neither, it logs an error and does not start the listener.
- **The web UI is a separate download on Linux (`.deb` / `.rpm`).** Run
  `sudo nscp web install-ui` after installing the package to fetch the matching
  UI bundle (`nscp web ui-status` / `uninstall-ui` manage it). Until then the
  web port serves a built-in placeholder; the REST API and every listener work
  without it. The Windows MSI still bundles the UI inline.
- **Building for a custom location** — the Linux layout follows the FHS and the
  install prefix (daemon `/usr/sbin/nscp`, modules and private libs under
  `/usr/lib/nsclient`, config `/etc/nsclient`, state and logs under `/var`).
  Patching hardcoded paths is no longer needed: pass `-DCMAKE_INSTALL_PREFIX`
  and the standard `CMAKE_INSTALL_*DIR` knobs. To point an installed daemon at a
  `boot.ini` elsewhere, use
  `nscp service --run --path-override boot-conf=/etc/nsclient/boot.ini`.

## 0.12.6

### Requires action

- **`cache allowed hosts` is a real boolean now.** It used to be parsed as a
  string with surprising truthiness: change `yes` / `on` to `true`. Numeric
  `1` / `0` still work.
- **NRPEServer survives a failed listener** (bad bind address, port in use): it
  logs an ERROR and stays loaded with no active listener instead of failing the
  whole module, so you can reconfigure and reload without restarting the
  service. If you alerted on "module load failed", add "NRPE listener failed" as
  a separate signal.

### Changes behaviour (no action)

- **Nagios range syntax in performance data** is additive — plain numbers still
  parse as before. Only consumers that special-cased NSClient++'s old output may
  need adjusting.

### New and opt-in (no action)

- 🔒 **Permission policy layer** — a new system, disabled by default. Existing
  installs behave exactly as before until an operator sets
  `/settings/permissions/enabled = true`. If you opt in: per-command rules under
  `/settings/permissions/policies` apply to **queries only** — exec is gated by
  the separate `allow exec` boolean, which defaults `true`, so enabling the
  policy does not silently break the WEB scripts UI, lua/python
  `core:simple_exec(...)` or CLI exec. Roll out with `log allows = true` first to
  inventory real traffic. See [Permissions](../concepts/permissions.md).
- 🔒 **NRPEServer `client identity source`** — a new setting defaulting to
  `none`, which is the previous behaviour (subject is bare `NRPEServer`). Set it
  to `cn` only for per-cert principals, and only after configuring
  `verify_mode = peer-cert` and a `ca path` pinned to your **private**
  monitoring CA — the system trust store would accept any public cert's CN. The
  module refuses to start with a clear error if `cn` is set without those.

## 0.12.5

### Requires action

- **`[/paths]` users** — copy the entries into a `[paths]` section in `boot.ini`
  (same section name, different file, next to `nscp.exe`) and delete the old
  section from `nsclient.ini`. There is no automatic migration and the
  settings-side section is no longer consulted. The default install does not use
  `[/paths]` and is unaffected.
- **Custom-plugin authors** — implement the new optional `prepare_shutdown`
  callback if your module manages sockets or background threads. `unload` is now
  a last-resort teardown rather than the place where listeners get stopped.

### New and opt-in (no action)

- 🔒 **WEB `disable admin user = true`** — suppresses the built-in admin even on
  first boot, for monitoring-only exposure of the WEB UI; define your own
  read-only users, or a tightly scoped `anonymous` role. Existing installs keep
  their admin and work unchanged.

## 0.12.4

### Changes behaviour (no action)

- 🔒 **Icinga `check_nscp_api` works again** after the upgrade, with no config
  change. For a non-stock probe with a different binary name, set
  `[/settings/WEB/server] legacy query auth user agents` to a substring of its
  User-Agent (or to plain `Icinga` to broaden the match). For the strictest
  behaviour — no query-string credentials at all — set that key to empty.

## 0.12.2

### Requires action

- 🔒 **`allowed hosts` is fail-closed.** An empty value used to mean "allow any
  source" and now rejects every connection, with the reason logged. Audit it on
  every node; to genuinely accept any source, say so explicitly:

  ```ini
  allowed hosts = 0.0.0.0/0,::/0
  ```

- **Scheduler cron expressions evaluate in local time** (#570), matching
  standard cron semantics — `40 15 * * *` now fires at 15:40 host-local instead
  of 15:40 UTC. Update the expressions, or restore the old reference clock with
  `[/settings/scheduler] timezone = utc`. Any POSIX TZ string is accepted; IANA
  names such as `Europe/Stockholm` are **not** — an unparseable value falls back
  to UTC and shows as `UTC?` in timezone labels.
- 🔒 **`check_nt` (NSClientServer) defaults to `ssl = true`.** The listener still
  starts with TLS off, but logs a warning at startup and another when a password
  is configured. Set `ssl = false` explicitly in `[/settings/NSClient/server]`
  if your clients do not speak TLS — and prefer NRPE or NSCA-ng.
- 🔒 **`check_nt`: the literal password `None` no longer authenticates.** A
  client sending `None` used to be accepted when no server password was
  configured; an empty server password now rejects every request. Configure a
  password, or move to a modern protocol.
- 🔒 **Passwords and tokens are no longer accepted in the query string** of GET
  queries — move them to a header. (The `/auth/token` and `/auth/logout`
  endpoints themselves still exist, behind the WEB `legacy` permission; see
  0.16.2.)
- 🔒 **Outbound SSL connections verify the certificate hostname** whenever
  `verify_mode` includes `peer`. A peer certificate whose CN/SAN does not match
  the host the client connects to is now rejected, where it used to pass on a
  valid chain alone. Re-issue certificates that are reached under a name they do
  not carry, or connect using the name in the certificate.

### Changes behaviour (no action)

- 🔒 **NRPEServer logs an ERROR at startup when `insecure = true`** — it names
  the loss of server authentication ("Clients have no way to authenticate this
  server, traffic can be MITMed"). Nothing else changes for that listener, but
  the message will show up in dashboards that alert on ERROR: whitelist it on
  agents deliberately run insecure. The insecure cipher preset also stops
  allowing anonymous (ADH) suites, which legacy `check_nrpe` does not need.

## 0.12.1

### Changes behaviour (no action)

- **`check_service`: `delayed` is reported only for auto-start services**
  (#362). A filter matching `start_type = 'delayed'` no longer picks up
  `Manual`, `Boot`, `System` or `Disabled` services. To alert on "not running
  and not disabled", write
  `start_type IN ('auto','delayed','boot','system') AND state != 'running'`.
- **`check_service`: `${desc}` returns the real display name** instead of the
  literal `TODO` (#456). Backends that matched on `TODO` need a different
  signal.
- **`check_service`: `perf-syntax=none` actually suppresses perfdata** (#681),
  where it used to be ignored and emit empty-aliased entries.

## 0.12.0

### Requires action

- **Mixed `warn=` / `crit=` expressions now evaluate when nothing matches.** An
  empty result set used to be skipped and return OK; object-bound variables now
  default to `false` and summary variables hold their final values, so
  `crit = state = 'stopped' OR count = 0` becomes CRITICAL on an empty set. Add
  a `count > 0 AND …` guard if a config relied on "empty means OK".
- **Module authors:** `http::request` and `http::response` are distinct types,
  headers are stored case-insensitively and chunked decoding is transparent.
  Out-of-tree modules built against the old shared request/response type need a
  small adjustment to compile.

### Changes behaviour (no action)

- **`warn=` / `crit=` no longer fire mid-iteration on running counts.** The
  verdict is computed against the final counts, so a config tuned against the
  old early-fire behaves differently — `crit = state = 'hung' OR count < 5` used
  to trip on the first row.
- **Realtime `check_process` matches `process=` case-insensitively**, like the
  active path always did (#587, #552). A rule that deliberately matched one
  casing now matches both.

## 0.11.33

### Changes behaviour (no action)

- **`check_files` corner cases are fixed**: `max-depth=0` scans the top
  directory instead of returning empty (#730); missing paths return UNKNOWN
  instead of OK/empty (#613); junction loops are no longer double-counted
  (#605); legacy `CheckFiles` calls that returned UNKNOWN on empty results now
  return OK (#717). Review alerting that relied on any of these.

### New and opt-in (no action)

- **Proxy support** — the new `proxy` and `no-proxy` keys and the `[proxy]`
  section in `boot.ini` are opt-in. No configuration migration is required.

## 0.11.32

### Changes behaviour (no action)

- **The documentation tree was reorganised**, so bookmarks and deep links into
  the old structure may no longer resolve.
