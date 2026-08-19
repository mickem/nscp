# Upgrading

What to do when upgrading NSClient++, newest release first. Most upgrades are
in place — defaults are preserved and the default install is usually unaffected
— but the items below change observable behaviour or want a configuration
touch. Read the entries between the version you are on and the version you are
moving to.

Items marked 🔒 are security-relevant; the [Security notices](../security/notices.md)
page tracks those in one place. Full per-release detail lives in each
[GitHub release](https://github.com/mickem/nscp/releases).

---

## 0.16.2

- 🔒 **Sensitive settings values are redacted on read.** The REST settings
  read endpoints (`GET /api/v2/settings/...`, `/descriptions`) and
  `nscp settings --list` / `--show` now return `***` for keys registered
  sensitive, matching the `diff` endpoint. No action required; tooling that
  read such a value back now receives `***`. See
  [Security notices](../security/notices.md).
- 🔒 **The built-in `legacy` WEB role is no longer seeded on fresh installs,**
  and any role granting the `legacy` permission now triggers a `SECURITY`
  warning at startup (and from `nscp web add-role` / `add-user`). Existing
  installs are unaffected — the role stays in their config. The `legacy` grant
  unlocks the deprecated `/query.pb` and `/query/{name}` query-dispatch
  endpoints, so a token with it can run any registered check/command; only
  grant it to trusted legacy systems. See
  [Security notices](../security/notices.md).

## 0.16.1

- **RHEL/SUSE:** workaround `ca=` arguments can be dropped — `${ca-path}` now
  resolves on its own (the explicit form still works). Packagers cross-building
  for another distribution should set `-DCONFIG_CA_PATH=`.
- **`check_logfile`** is unchanged unless you opt in to `bookmark` / `max-lines`.
  Adopting `bookmark` is a trade-off: a line is consumed when the check runs
  (not when its result is submitted) and positions are saved on clean shutdown,
  so a crash re-reports the backlog. Prefer an explicit bookmark name for a
  check whose filter changes often.
- **Settings URLs with a query string now send it.** A server that relied on
  receiving the bare path will now see the parameters. The offline-boot cache
  file is migrated to the query-aware name once on first start.
- **`${hostname}` in an existing config changes meaning** — it is now expanded
  everywhere `expand_hostname` is used (including submit clients' `hostname`),
  where it used to be left as literal text.
- **`run on startup` is off by default** so the default install is unaffected;
  if you enable it for the `default` schedule use `startup window` to avoid a
  thundering herd.
- **Building the HTML docs on non-Windows** now needs `-DNSCP_BUILD_DOCS_HTML=ON`.

## 0.14.1

- **Licence change:** now distributed as **Apache-2.0 OR GPL-2.0-only** — a
  clarification/relicensing with no code or runtime behaviour change. Review it
  if your organisation tracks bundled-software licences.
- **CheckNet perfdata is on by default.** If you added perfdata manually,
  make sure you are not now emitting it twice.
- **Boolean check arguments** (`option=true` / `option=false`) now work from the
  CLI as well as REST; bare-flag usage is unchanged.
- 🔒 **`CheckSecurity` is not loaded by default.** Enable it before using its
  checks (`nscp settings --active-module CheckSecurity`). Windows-only checks
  return UNKNOWN elsewhere.

## 0.12.6

- 🔒 **New permission policy layer, disabled by default.** Existing installs
  behave exactly as before until an operator sets
  `/settings/permissions/enabled = true`. If you opt in: per-command rules apply
  to **queries only** (exec is gated by the separate `allow exec` boolean, which
  defaults `true`); roll out with `log allows = true` first to inventory real
  traffic. See [Permissions](../concepts/permissions.md).
- 🔒 **NRPEServer `client identity source`** defaults to `none` (previous
  behaviour). Set to `cn` only after configuring `verify_mode = peer-cert` and a
  `ca path` pinned to your **private** monitoring CA — the system trust store
  would accept any public cert's CN.
- **`[/paths]` overrides** from an older install moved to `[paths]` in
  `boot.ini` (same section name, different file). No automatic migration — copy
  each entry across and delete the old section.
- 🔒 **WEB `disable admin user = true`** is a new opt-in for status-only WEB
  exposure; existing installs keep their admin unchanged.
- **NRPEServer** now survives a failed listener (logs an ERROR, leaves the
  module loaded) instead of failing the whole module. Add "NRPE listener failed"
  as a signal if you alerted on module-load failure.
- **`insecure = true` on NRPEServer** now logs at ERROR (louder, behaviour
  unchanged) — whitelist the message on agents intentionally run insecure.

## 0.12.5

- **`[/paths]` users:** copy entries into `[paths]` in `boot.ini`; the
  settings-side section is no longer consulted. Default installs are unaffected.
- **Custom-plugin authors:** implement the new optional `prepare_shutdown`
  callback if your module manages sockets or background threads — `unload` is
  now a last-resort teardown.
- 🔒 **Monitoring-only WEB deployments:** `disable admin user = true` under
  `[/settings/WEB/server]` suppresses the built-in admin even on first boot;
  define your own read-only users (or a tightly scoped `anonymous` role).

## 0.12.4

- 🔒 **Icinga `check_nscp_api`** works again after upgrade with no config
  change. For a non-stock probe, set `[/settings/WEB/server] legacy query auth
  user agents` to a substring of its User-Agent. For the strict 0.12.3 behaviour
  (no query-string credentials at all), set that key to empty.

## 0.12.3

1. 🔒 **Audit `allowed hosts`** on every node — empty values now reject everything.
2. 🔒 **`check_nt` (NSClientServer)** now defaults to `ssl = true`; set
   `ssl = false` explicitly if your clients don't speak TLS.
3. 🔒 **Replace clients** that call `/auth/token` or `/auth/logout` with the
   `/api/v2/login` flow, and any that pass `?TOKEN=` / `?__TOKEN=` in the query
   string with a header-based token.
4. **Scheduler cron expressions** on non-UTC hosts shift to local time — update
   them or set `[/settings/scheduler] timezone = utc`.
5. **Review `check_service` / `check_process` / `check_files` filters** that
   relied on the old (now corrected) behaviours.
6. Restart and review the log for new "refused alias" / "rejected connection"
   warnings — configurations that were previously silently accepted.

## 0.11.33

- No configuration migration required (new `proxy` keys are opt-in). The
  `check_files` fixes change a few corner cases: `max-depth=0` now scans the top
  directory (#730); missing paths return UNKNOWN (#613); junction loops are not
  double-counted (#605); empty results return OK instead of UNKNOWN (#717).
  Review alerting that relied on the old corner-case behaviour.
