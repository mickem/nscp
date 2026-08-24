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

## 0.17.0

- **Check-specific filter keywords that clashed with the generic summary
  keywords are renamed.** A handful of checks registered their own keyword
  named `status`, `count` or `total` — the same names as the built-in summary
  keywords (`%(status)`, `%(count)`, …). The check-specific value won in
  `filter`/`warning`/`critical` and `detail-syntax`, while `top-syntax` and
  the reference documentation showed the generic one, which was confusing.
  Each such keyword now has a distinct, documented name:

    | Check | Old | New |
    |-------|-----|-----|
    | `check_cpu`, `check_cpu_utilization` | `total` | `usage` |
    | `check_battery` | `status` | `battery_status` |
    | `check_network` | `status`, `total` | `link_status`, `throughput` |
    | `check_os_updates` | `count` | `updates` |
    | `check_patch_age` | `count` | `patches` |
    | `check_pending_reboot` | `count` | `signals` |
    | `check_printjobs` | `status` | `job_status` |
    | `check_printqueue` | `status` | `printer_status` |
    | `check_installed_software` (Linux) | `status` | `package_status` |
    | `check_activation` | `status` | `activation_status` |
    | `check_docker` | `status` | `container_status` |
    | `check_connections` | `count`, `total` | `connections`, `total_connections` |
    | `check_dns` | `count` | `records` |
    | `check_http` | `status` | `status_message` |
    | `check_shadowcopy` | `count` | `copies` |
    | `check_disk_health` | `total` | `size` |

  **Existing configurations keep working:** the old names remain as
  undocumented deprecated aliases with unchanged behaviour, so filters like
  `check_cpu "warn=total > 80"` still work. Migrate to the new names at your
  convenience; only they appear in the reference documentation. A few
  behaviour changes ride along:

    - `check_os_updates`' default output now reports the actual number of
      updates (rendered from the detail line via `${list}`) instead of the
      matched-row count the old default showed.
    - Three default perfdata keys change because the default `perf-config`
      lists the renamed keyword: `check_cpu_utilization` (Linux)
      `cpu_total` → `cpu_usage`, `check_patch_age` `patch_count` →
      `patch_patches`, `check_pending_reboot` `reboot_count` →
      `reboot_signals`. Pass your own `perf-config=extra(...)` with the old
      (alias) keyword to keep the old key. All other perfdata keys are
      unchanged (the renames keep their original perf suffixes, and
      `check_connections`' default perf-config intentionally still uses the
      alias for series continuity).
- 🔒 **`${host}` and friends now resolve in attachment paths and in
  `[/includes]`.** Host name placeholders were only expanded in settings urls
  and in the url an attachment is fetched from, not in the path it is written
  to nor in an included file name. An unknown `${...}` token in a path is not
  an error - it resolves to the installation directory - so a configuration
  such as `[/attachments] ${shared-path}/${host}.ini = ...` did not fail, it
  quietly wrote one file with the installation directory in its name. Those
  paths now name the host, which changes where such a file lands: check any
  `${host}`, `${hostname}` or `${domain}` you already have under
  `[/attachments]` or `[/includes]`, and remove the workaround if you scripted
  around this. Configurations without a host name placeholder are unaffected.
  When the substitution lands in a local path, the value is sanitized to the
  characters a legal host name can contain (see the
  [security notice](../security/notices.md#host-name-placeholders-are-sanitized-before-they-land-in-a-local-path)).
  Like `nscp settings --switch`, `nscp settings --migrate-to` (and the REST
  migrate) now keeps a placeholder you pass it as-is in `boot.ini` while
  migrating into the expanded per-host file, so the template survives on a
  fleet-managed machine.
- **Check messages can now be told how to render their numbers.** Every filter
  check gained four options - `decimals`, `byte-unit`, `decimal-separator` and
  `thousands-separator` - so `check_drivesize` can report
  `141.06GB/1006.85GB` (or `141,06GB/1.006,85GB`) instead of
  `140.293GB/0.983TB`. The defaults are unchanged, so an installation that does
  not set them renders exactly what it rendered before. The options only touch
  the message: performance data keeps its full precision and its `.` radix, and
  so do the numbers you write in a filter or a threshold. Real-time filters take
  the same settings as `decimals`, `byte unit`, `decimal separator` and
  `thousands separator` keys, inheritable from the default template. `decimals`
  is capped at 15 (a `double` carries no more than that): the query option and
  the `format_bytes()`/`format_number()` argument reject a larger value, and the
  settings key clamps it, so a typo like `decimals=1000000` can no longer make a
  check try to render a multi-megabyte number.
  Note that setting **any** of the four options also moves plain float keywords
  in the message onto the number format: with `decimals` unset they then render
  with up to three decimals (trailing zeros stripped) instead of the legacy
  6-significant-digit form — `2.71094` becomes `2.711`, and large values stop
  rendering scientific (`1.23457e+07`). A pipeline that matches float text in
  the message may need its pattern relaxed when you first set one of these
  options; leave all four unset and the message is byte-for-byte unchanged.
- **An unknown unit in `format_bytes()` is now reported instead of rendering
  nonsense.** `format_bytes(used, 'gb')` used to render `1.27055e-10` because
  the unit comparison was case sensitive, and any misspelled unit rendered
  `value/1024^7`. Lowercase units now work, and a unit that names nothing (say
  `'ZB'`) makes the check report `Filter processing failed: format_bytes
  failed: Unknown byte unit: ZB`. A syntax string with such a typo returns
  UNKNOWN rather than a quietly wrong number - fix the unit, or the check will
  stay UNKNOWN.
- **A `unit:` in `perf-config` that names no unit no longer divides the metric
  by 1024⁷.** An unrecognised unit now leaves the value alone. If a graph of
  yours has been flat at a near-zero value, check the `unit:` spelling in its
  `perf-config`: the metric will jump to its real magnitude on upgrade.
- **`perf-config`'s `unit:` now converts plain byte series instead of
  relabelling them.** On series that are byte counts but do not auto-scale
  (most byte keywords outside `check_drivesize`), `unit:KB` used to change the
  label only, shipping `=1536KB` for a value of 1536 *bytes* - a metric off by
  the unit ratio to any consumer that trusts the label. The value and the
  warn/crit bounds now convert into the requested unit, matching what the
  auto-scaling series always did. A dashboard that compensated for the
  mislabelling will see the metric drop by that ratio on upgrade. Series not
  measured in bytes (`ms`, `%`, `s`, ...) and explicit `minimum:`/`maximum:`
  overrides are unaffected, and a `unit:` that names no byte unit still only
  changes the label.
- **Errors raised while a template renders are now reported.** A function that
  failed inside `detail-syntax` or `top-syntax` used to leave the placeholder
  empty and say nothing; the check now returns UNKNOWN with `Filter processing
  failed: …`. This surfaces template mistakes that have been silently producing
  incomplete messages.
- **`check_pending_reboot`'s default message now names the pending-since
  time.** When the reboot was queued by Component Based Servicing or Windows
  Update, the message gains a suffix: `Reboot required: Windows Update` became
  `Reboot required: Windows Update (pending since 2026-08-16 09:41:12)`.
  Notification pipelines that match the exact message text (an anchored regex,
  a string equality) need their pattern relaxed; thresholds, states and
  existing keywords are unchanged.
- **Filter comparisons between a text keyword and a bare number are now
  numeric.** A string-typed keyword compared against an unquoted number used
  to order *lexically* — `filter=value > 90` on `filter_perf` matched
  `value=100` as false ("100" sorts before "90") — or, with the operands
  reversed (`90 > value`), failed to evaluate at all. Both now compare as
  numbers, whichever side the keyword is on: the row's text is parsed per
  record, and a value that is not a number simply never matches (the check
  logs one warning naming the value; the result stays a certain
  non-match, not UNKNOWN). This applies to keywords such as
  `value`/`warn`/`crit`/`min`/`max` (`filter_perf`, `render_perf`),
  `speed` (`check_network`), `string_value` (`check_registry_value`) and the
  `column()` function (`check_logfile`). **Quoted** literals keep the lexical
  comparison — `version < '8'` still orders as text — as do `like`, `regexp`
  and `in`, keyword-specific converters (`state = 'running'`,
  `age > 30m`), and the `= 'unknown'` / `= 'never'` sentinels for optional
  values. Review any filter that deliberately relied on text ordering
  against a bare number: quote the number to keep the old behaviour.
- **Fractional numbers in thresholds are no longer truncated or rounded.**
  `count > 2.5` used to evaluate as `count > 3` (the literal was rounded
  into the counter's integer domain); unit literals lost their fraction
  entirely, so `working_set > 1.5g` meant 1g and `uptime < 2.5h` meant 2h.
  Fractions now mean what they say. Whole-number thresholds are unchanged;
  only expressions that already used a decimal point can behave differently.
- **`filter_perf`/`render_perf`/`xform_perf`: the `max` and `min` filter
  keywords were swapped.** `max` read the perf-data *minimum* bound and
  `min` the *maximum*. They now read the bounds they name — a filter that
  compensated for the swap needs the two names exchanged back.
- **Syslog submission works again, so a configured syslog server will start
  receiving traffic.** `SyslogClient` read its connection settings from the
  wrong place, so the target's address, port, facility, severity and templates
  were all ignored: the agent logged `Undefined facility:` and sent nothing.
  Broken since 0.4.3 (2015). If you have a syslog target configured, check it
  still points where you want before upgrading - it has not been delivering,
  and it will now. `CheckMKClient` had the same defect on its query path.
- **SMTP notifications now announce this host in EHLO instead of
  `localhost`.** The sender's host name was read from the wrong place, so it
  was always empty and the EHLO fell back to `localhost`. If your mail server
  applies HELO/EHLO policy (SPF checks, or a rule that rejects `localhost`),
  the agent will now identify itself properly - set `ehlo-hostname` on the
  target if you need a specific name.
- **`nscp settings --show` now says so when `--key` is missing.** `--show
  --path /some/path` without a `--key` used to print nothing and exit 0; it
  now reports `Invalid command line please use --path and --key with show`
  and exits non-zero. A bare `--show` still describes the active
  settings store, and `--show --path … --key …` is unchanged. Scripts that
  relied on the silent success need the missing `--key` added.
- **Client commands shorter than eight characters work again.** A command
  such as `cpu` or `run` answered `Exception processing command line:
  basic_string::substr …` instead of running, in every module built on the
  shared client machinery (NRPE, NSCA, NRDP, Graphite, …). Remove any
  workaround that renamed such commands to a longer alias; no configuration
  change is needed.
- **The settings diff no longer reports changes that were already saved.**
  `get_changes()` — behind the REST settings `diff` endpoint and any
  operator-facing "what am I about to save?" view — kept listing an edit for
  the lifetime of the process after it had been written, reporting it as a
  `modified` entry whose old value equalled its new one. Tooling that treated
  a non-empty diff as "unsaved work pending" no longer needs to special-case
  that; no configuration change is needed.

## 0.16.4

- 🔒 **The bundled Mongoose web server is upgraded to 7.23,** fixing two
  critical (CVSS 9.1) HTTP request-smuggling vulnerabilities in its HTTP
  parser ([CVE-2026-73256](https://nvd.nist.gov/vuln/detail/CVE-2026-73256),
  [CVE-2026-73257](https://nvd.nist.gov/vuln/detail/CVE-2026-73257)). This
  affects the **Windows builds** of the `WEBServer` module (REST API / web UI)
  and is exploitable when NSClient++ sits behind a reverse proxy or WAF —
  upgrade promptly in that topology. The Linux packages use the Boost.Beast
  backend and are unaffected. No configuration change is needed. See
  [Security notices](../security/notices.md).

## 0.16.3

- **`check_nt` (NSClientServer) answers the real nagios-plugins client again.**
  Requests without a trailing newline used to hang until the client timed out
  (`No data was received from host!`) — broken since 0.12.2. Remove any
  client-side timeout/retry workarounds; no configuration change is needed.
  If you expose this legacy endpoint, see the new guidance on securing it
  (password, `allowed hosts` and the `allow` command list).

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
