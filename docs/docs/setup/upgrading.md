# Upgrading

What to do when upgrading NSClient++, newest release first. Most upgrades are
in place — defaults are preserved and the default install is usually unaffected
— but the items below change observable behaviour or want a configuration
touch. Read the entries between the version you are on and the version you are
moving to.

Each entry carries an icon for the area it touches. 🔒 is the one that
matters: those entries are security-relevant, and the
[Security notices](../security/notices.md) page tracks them in one place. Full
per-release detail lives in each
[GitHub release](https://github.com/mickem/nscp/releases).

---

## 0.18.1

- 🔒 **Icinga API submissions now honour the configured `timeout`, and
  credentials no longer reach the trace log.** The `IcingaClient` module's
  HTTP calls previously waited forever — the target's `timeout` setting
  (default 30 s) was read but never applied — so a stalled Icinga endpoint
  could silently wedge passive-result submission; set `timeout = 0` on the
  target if you depend on the old unbounded wait. The same pass masked
  `password`/`token` values in the trace-level target dump (this also covers
  the other client modules sharing that machinery) and added a log message
  when an `https` submission runs with certificate verification disabled
  (`verify mode` empty or `none`). See
  [Security notices](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
- 🔒 **Filter expressions are now bounded in length and nesting depth.** A
  `filter` / `warning` / `critical` expression — and a `%(...)` expression
  placeholder inside a syntax template — longer than **1024 characters** or
  nested more than **64** parentheses deep is now rejected at parse time
  instead of being evaluated, failing with a clear "exceeds the maximum
  length/depth" error. The where-parser and the expression evaluator both
  recurse with the shape of the input, so an unbounded or deeply nested
  expression could exhaust the stack and crash the agent. Real filters are a
  small fraction of these limits, so the default install and every normal
  configuration are unaffected — only a pathologically large or deeply nested
  expression is refused. See the
  [security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
- 🔒 **CheckExternalScripts hardening.** A hardening pass over the external
  scripts module tightened several rough edges: the `ext-scr install` argument
  lockdown now writes to the setting the module actually reads (previously it
  was a no-op, so a lockdown could silently not apply), the command timeout is
  enforced on every execution path with output capped, the `show`/`delete`
  sandbox resolves symlinks, and `%`/`^` are blocked on the shell-fallback path.
  The default install is unaffected (arguments are off by default). If you rely
  on `ext-scr install` to disable arguments, re-run it after upgrading so the
  effective setting is written. See the
  [security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
- 🔒 **NSCA-NG cert mode now actually verifies the server (and presents the
  client certificate).** In 0.18.0 the `NSCANgClient` cert mode
  (`use psk = false`) applied its TLS configuration to the OpenSSL context
  *after* the connection object had been created from it, and OpenSSL copies
  the verify mode, client certificate, cipher list and TLS-version bounds out
  of the context at creation time — so `verify mode = peer-cert` was silently
  ignored, any certificate the server presented was accepted, and the
  configured client certificate was never sent. The configuration is applied
  before the connection is created now. Two operator-visible consequences:
  a cert-mode target that "worked" against a server whose certificate does
  not chain to the configured `ca` (or does not match the host name) will now
  fail to connect — that is the verification working; fix the server
  certificate, or opt out explicitly with `insecure = true` if you accept the
  MITM risk. And servers that require a client certificate will start seeing
  it. The default PSK mode (`use psk = true`) is unaffected. See
  [Security notices](../security/notices.md).
- 🔒 **NSCA: an unrecognized `encryption` value is now a hard error instead of
  silently running without encryption.** A typo'd algorithm name (`aes-256`),
  or one not compiled into the build, used to fall back to *no encryption* on
  the end carrying it. Since the ciphers must match, a one-sided typo showed
  up as the peer rejecting every submission with a CRC error rather than as
  accepted plaintext — but that failure gave no hint of its cause, and a
  value broken the same way on both ends did run plaintext while looking
  encrypted. Now the `NSCAServer` module refuses to load and an `NSCAClient`
  submission fails, each naming the problem and listing the available
  algorithms. Default installs (`aes256`) are unaffected. **Breaking** only
  for setups relying on the fallback: fix the algorithm name, or set
  `encryption = none` explicitly if plaintext was intended — this includes
  builds compiled without crypto++, where any cipher name previously
  degraded to plaintext and the server now refuses to start.
  See the [security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
- 🔒 **NSCA hardening: empty-password warning, `performance data = false`
  honoured, wire-field validation.** Enabling NSCA encryption with an empty
  `password` now logs an error on both ends (the password *is* the key, so an
  empty one is a well-known key) — set the same password on both ends to
  clear it. `NSCAServer`'s `performance data = false` now actually strips
  perfdata from forwarded submissions (it was silently ignored). Inbound
  host/service names are stripped of control characters and out-of-range
  status codes are clamped to UNKNOWN. No action needed on a default install.
  See the [security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
- 🔒 **NRDP submissions now honour `timeout` and `retry`.** Both settings were
  parsed but never applied: a submission to a server that accepted the
  connection and then stalled hung the submission thread forever, and failed
  submissions were never retried. Every step of the exchange (connect, TLS
  handshake, proxy tunnel, request, response) now runs under the configured
  `timeout` (default 30 seconds for configured targets, 10 for one-shot
  `nscp client` submissions), and transport failures are retried up to `retry`
  times. Nothing to do unless your NRDP endpoint legitimately takes longer
  than the timeout to answer — raise `timeout` on that target. The response
  body is also capped at 5 MB, far above any real NRDP reply. See the
  [security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
- 🔒 **A `tls version` with a trailing `+` now means "that version or later".**
  `1.2+` (the common default) previously negotiated TLS 1.2 *only*; it now
  also permits TLS 1.3, and `any` is accepted as the documentation always
  claimed. This applies everywhere the setting exists: NRDP and the other
  HTTP-based clients, the NRPE/NSCA clients and servers, and `check_tcp`.
  No action needed; pin an exact version (`tls version = 1.2`) if a peer
  misbehaves when TLS 1.3 is offered. See the
  [security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
- 🔒 **The Elastic module now verifies HTTPS server certificates and can
  authenticate.** `ElasticClient` previously hardcoded TLS verification off;
  an `https://` address now defaults to `verify mode = peer` against the
  platform CA bundle, with new `tls version`, `verify mode` and `ca` settings
  to tune it. New `user`/`password` and `api key` settings authenticate
  against secured clusters (Elasticsearch 8+ defaults), and a new `timeout`
  (default 30s) bounds each submission. If you rely on a self-signed
  certificate, point `ca` at it or set `verify mode = none` explicitly. See
  [Security notices](../security/notices.md).
- 📤 **The Elastic module no longer sends the legacy `_type` parameter by
  default.** Mapping types were removed in Elasticsearch 8, which rejects
  bulk requests carrying them, so the `event type`, `metrics type` and
  `nsclient log type` defaults are now empty. Only Elasticsearch 6.x or older
  needs them: set the old values (`eventlog`, `metrics`, `nsclient log`)
  explicitly to keep the previous behaviour. Batched documents also now get
  distinct ids — previously all documents in one bulk request shared an id
  and overwrote each other, so multi-entry events show up completely now.
- 🔒 **check_mk client targets: configured TLS settings are honoured again.**
  `check_mk_target_object::read()` added the SSL keys (`use ssl`,
  `certificate`, `verify mode`, `ca`, …) to its settings registry but never
  called `register_all()`/`notify()`, so values set on a
  `[/settings/check_mk/client/targets/…]` section were silently ignored and
  the client connected in plaintext regardless of configuration. The keys are
  read (and documented) again. If you configured `use ssl = true` on a
  check_mk target, the connection becomes TLS on upgrade — make sure the
  server side actually speaks TLS, or the check starts failing. Details in the
  [security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
- 📨 **Syslog client targets: configured severities and templates are honoured
  again.** The same defect existed in the syslog client's target object: the
  `severity`, `facility`, `tag_syntax`, `message_syntax` and per-status
  severity keys on a `[/settings/syslog/client/targets/…]` section were never
  read, so the built-in defaults (`error`/`kernel`/…) always won. Values you
  configured — perhaps years ago, without effect — now apply; if your syslog
  routing depends on the previously effective defaults, review the target
  sections for stale keys.
- 📨 🔒 **Syslog messages now carry the RFC 3164 HOSTNAME field, and several
  syslog options work for the first time** (see the
  [security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework)).
  What changes on the wire and in behaviour:

    - Datagrams now read `<PRI>TIMESTAMP HOSTNAME TAG MESSAGE`. The `hostname`
      setting under `[/settings/syslog/client]` — until now read but never
      used — fills the HOSTNAME field (default `auto`, the machine name).
      Receivers that promoted the tag (default `NSCA`) to origin host will now
      file records under the real host name; adjust any log-parsing rule that
      keyed on the old, hostname-less format.
    - The `tag_syntax` and `message_syntax` target settings now reach the
      wire. They were stored under keys the sender never read, so a
      settings-defined target sent an empty tag and dropped the message text.
    - The per-state severity options (`ok-severity`, `warning-severity`,
      `critical-severity`, `unknown-severity`) passed as command arguments now
      take effect; they too were stored under keys that were never read.
    - An unknown `severity` or `facility` name now degrades to priority `<13>`
      (user.notice) instead of `<0>` — which is kernel.**emergency**, a
      priority many receivers page or broadcast on. A missing per-state
      severity now falls back to the base `severity` instead of tripping that
      fallback.
    - All C0 control bytes and DEL in the outgoing line are replaced with
      spaces (previously only CR, LF and NUL), so check output cannot smuggle
      ANSI escape sequences into the receiver's log.

- ⏱️ **The Graphite `timeout` is now enforced — as a budget for the whole
  submission.** It was doubly dead before: the value the operator configured
  never reached the connection (the lookup read a map the well-known `timeout`
  key is not stored in, so the default 30 always won), and the connection did
  not use even that — resolution, connect, the TLS handshake and every write
  ran with no deadline, so a stalled carbon endpoint could hold the submitting
  thread for the OS-level TCP timeout, or indefinitely on a stuck write. The
  configured `timeout` (default 30s) is read now and bounds the whole
  submission as one budget, like the SMTP client's. A target whose submissions
  previously completed by quietly taking longer will now fail at the
  configured value; raise `timeout` on targets talking to a slow relay. See
  [Security notices](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
- 🧹 **The Graphite `retry` setting is not honoured and is no longer read.**
  The module always made exactly one attempt per submission; reading the value
  made it look otherwise, and a retry loop would multiply the worst-case time
  a stalled endpoint can hold the submitting thread by the retry count.
  `retry`/`retries` are registered for every client module centrally, so they
  still appear in the GraphiteClient reference, but `GraphiteClient` does not
  act on them. Mirrors the SMTP `retry` change in 0.18.0.

---

## 0.18.0

- 💥 **`check_nscp` is now a filter check, and it can see crash reports again.**
  Crash reporting itself has always worked — the agent has archived crash
  dumps since 0.4.x — but `check_nscp`'s *count* of them has not, for two
  independent reasons that accumulated over the years:

    - The count matched files whose extension equalled `txt`, while the helper
      it used returns the extension *with* its leading dot (`.txt`). That
      comparison has been false since the check was written in 0.4.2, so the
      crash count read 0 whatever was in the folder.
    - 0.6.10 dropped Google Breakpad, whose vendored submodule and build
      machinery had become a dependency burden, along with the separate
      crash-report sender tool that shipped with it. The handler that replaced
      it writes one plain-text `<timestamp>.crash` file per crash — the
      exception, the faulting address and the module it landed in — where
      breakpad left a `<guid>.dmp` minidump plus a `<guid>.dmp.txt`
      description. From 0.6.10 on, even a corrected `.txt` match would have
      found nothing.

  Separately, 0.4.3 stopped the module reading the archive folder from
  `[/settings/crash]` `archive folder` and hardcoded the compile-time default
  instead, so on any installation that had moved the archive folder the check
  was looking in the wrong place as well.

  All of that is fixed. Crash reports are recognised by `.crash`, and still by
  `.dmp` and `.txt` so pre-0.6.10 archives keep counting; the configured
  `archive folder` is read again; and `check_nscp` now exposes `crashes`,
  `last_crash`, `crash_age`, `errors`, `last_error`, `uptime`, `version` and
  `date` as filter keywords, accepts the usual `filter` / `warning` /
  `critical` and `top-syntax` / `detail-syntax` options, and emits perfdata for
  whichever keywords the thresholds name.

  The default verdict is unchanged — any crash report or any logged error is
  CRITICAL — but two things change for existing users. The message is now
  `N crash(es), M error(s), uptime <duration>` without the appended
  `last crash:` / `last error:` fragments. To put them back, pass a
  `detail-syntax`:

    ```
    check_nscp "detail-syntax=${crashes} crash(es) (${last_crash}), ${errors} error(s) (${last_error}), uptime ${uptime}"
    ```

  And because the crash count now works, an agent with an old report still
  sitting in the crash archive folder will start reporting CRITICAL where it
  previously read 0 — threshold on `crash_age` (for example
  `"crit=crash_age < 7d"`) if you only care about recent crashes, or clean the
  folder out. Crash reports remain a Windows-only concept; on Linux there is no
  crash handler, so `crashes` is always 0.

- 📤 **`check_and_forward` now actually submits the result.** The command ran the
  wrapped check, answered `Message submitted` and delivered nothing: the
  submission was built in a form the channels could not read, so NSCA, NRDP,
  Graphite and every other client module received a message with no results in
  it. It now submits like a scheduled check does. If you had given up on the
  command, it works — and if you built a workaround around its silence (for
  instance a schedule that exists only to be triggered), that workaround is no
  longer needed. The command also gained `channel` (`target` still works as a
  synonym), `alias`, `destination` and `source` options, and names the result
  after the command when no alias is given.

    **Breaking:** the command no longer accepts positional arguments for the
    wrapped command. `check_and_forward command=check_cpu warn=load>80` used to
    (try to) hand the bare trailing tokens to the wrapped command; it now fails
    to parse. Pass each wrapped-command argument through `arguments=` instead,
    one per argument:

    ```
    check_and_forward command=check_cpu "arguments=warn=load>80" "arguments=crit=load>90"
    ```

    The positional form had to go because its parser also swallowed the CLI's
    own `--argument key=value` tokens, which is what fed the wrapped command
    garbage and made it fail. If a submission is rejected by the channel the
    command now reports that failure instead of `Message submitted` — including
    when only one channel of a comma list (`channel=NSCA,GRAPHITE`) fails,
    which previously was silently reported as success.

- ⚙️ **A reload now loads modules enabled in an included file since the last one.**
  An `[/includes]` file is read once when the configuration is loaded and served
  from memory after that, so a module switched on in one — most importantly
  `fleet.ini`, which the fleet sync rewrites whenever a bundle changes — was not
  actually loaded until the service next restarted. Nothing said so: the file on
  disk was right and a fleet host reported itself in sync, while the module
  quietly did nothing. The visible case was a fleet bundle turning on
  `NRDPClient` and `Scheduler` to start passive submissions, which then never
  arrived. A reload now re-reads the included files first, so settings changed
  in them take effect and newly enabled modules are loaded; modules already
  running are left alone. Two things are unchanged: a module *disabled* since
  the last load keeps running until the service restarts, and `[/modules]` in
  the main `nsclient.ini` is still only read at startup.
- 📅 **New: run scheduled checks on demand with `run_schedules`.** After editing
  `nsclient.ini` you no longer have to wait out the interval to see the new
  result on the monitoring server — `nscp client --boot --query run_schedules`
  (optionally `--argument schedule=<alias>`) runs the configured schedules now
  and submits their results on their normal channel. It is a regular check
  command, so it also works over NRPE, REST and in `nscp test`. Nothing changes
  for existing configurations; the timers are untouched. See
  [Passive monitoring → Step 6](../scenarios/passive-monitoring-nsca.md#step-6-send-a-result-without-waiting-for-the-interval).
- 🚫 **A denied check is no longer submitted as a passive result.** The permission
  layer answers a denied query as a *successful* query carrying an UNKNOWN
  "Permission denied" payload, and both `run_schedules` and `check_and_forward`
  forwarded that to the monitoring server — overwriting the last real result
  for that service while telling the caller it had succeeded. Both now refuse
  with `Permission denied: not allowed to run <command>, nothing was
  submitted` and touch no channel. If you restrict what a REST or NRPE identity
  may run, expect the denial as an error where you previously saw a stale
  UNKNOWN appear on the server. See
  [Permissions](../concepts/permissions.md).
- 🔧 **`settings --update --add-defaults --use-samples` now writes the sample
  objects.** The flag was parsed and never read, so it behaved exactly like the
  plain invocation. It now writes the registered `/sample` sections, and
  `--remove-defaults` strips them again — the two are now exact inverses. An
  edited sample is still never overwritten or removed, and without the flag the
  output is byte-for-byte what it was. If you have scripted
  `--add-defaults --use-samples` expecting today's (sample-free) output, drop
  the flag.
- 💬 **`nscp client --query <cmd>` no longer appends "No module was specified…".**
  The line was appended to every result when no `--module` was named. Scripts
  that stripped or matched it can stop; naming a module is unchanged.
- 🔧 **Settings writes work again when `[/includes]` names a directory.** Saving
  handed the directory path to the INI writer, and the resulting "Is a
  directory" error aborted the whole save — so `nscp settings --set` and the
  web UI failed *and the main file was never written*. Directory includes are
  read-only by nature and are now skipped when saving. If you worked around
  this by removing a directory include, you can put it back.
- 🔒 **NRDP HTTPS submissions now verify the server certificate by default on
  the `nscp client`/REST path.** Previously an `https://` submission made that
  way with no `verify mode` set trusted any certificate silently (configured
  targets already defaulted to `peer`). If you submit to a self-signed NRDP
  endpoint that way and want to keep skipping verification, pass `--verify
  none` (or point `--ca` at the certificate) explicitly. Plain `http://` is
  unaffected. Two further NRDP hardening fixes ship in the same release — a
  malformed server response can no longer crash the agent, and the NRDP token
  and any proxy-URL credentials are redacted from the trace log. See
  [Security notices](../security/notices.md).
- 📡 **`--source-host` / `--sender-host` now name the sending host, on every
  client module.** They were registered against the *destination* container,
  where the well-known `host` key is routed into the typed address field — so
  naming a source host silently redirected the connection to it, and the
  sender the handler reads was never set. `SMTPClient` and `NRDPClient` had
  each worked around this by registering their own copies, which made the
  option name ambiguous and so **unusable on those two modules** (`option
  '--source-host' is ambiguous`). The options are registered once now, against
  the sender. If you had scripted around the old behaviour by passing
  `--source-host` to redirect a connection, use `--host` or `--address` for
  that instead.
- 🔒 **SMTP submissions now verify the server certificate against the agent's
  CA bundle.** `SMTPClient` relied on OpenSSL's built-in default verify paths,
  which on Windows do not include the Windows certificate store — so the
  default `security=starttls` failed verification against Gmail, Microsoft 365
  and every other public provider there, and `insecure-skip-verify` was the
  only way through. A new **`ca`** target setting (and `--ca` argument)
  defaults to `${ca-path}`, the same trusted bundle the other TLS clients use:
  the distribution's CA store on unix, the exported Windows ROOT store on
  Windows. **If a Windows SMTP target was only working because you set
  `insecure-skip-verify = true`, remove it and retry** — verification should
  now succeed. For an internal relay with a private CA, point `ca` at that
  bundle instead of waiving verification. Set `ca = none` to restore the old
  behaviour. A bundle that cannot be loaded now fails the submission with a
  message naming the file, rather than failing the handshake later with an
  unrelated-looking issuer error. A target that names no `ca` at all — a
  one-shot command line, or a default target — falls back to the same bundle,
  resolved once at module load, so no submission path is left on OpenSSL's
  built-in verify paths by accident.
- 🔒 **SMTP client security hardening.** The same pass closed a set of
  trust gaps in `SMTPClient`: data pipelined into the STARTTLS greeting is
  refused rather than trusted as post-handshake input, the EHLO name is
  validated for command injection before it reaches the wire, and ESMTP
  capabilities are matched per reply line instead of by substring search. No
  configuration change is needed. See
  [Security notices](../security/notices.md#smtp-client-security-hardening).
- ⏱️ **The SMTP `timeout` is now a budget for the whole submission** rather than a
  fresh deadline per operation (and per resolved address). A target that
  previously completed by using several times its configured `timeout` across
  a slow session will now give up at the configured value; raise `timeout` on
  targets talking to a slow relay.
- ✉️ **SMTP messages now carry a `Message-ID` header.** Nothing to do — it
  improves deliverability and gives mail administrators a handle to trace a
  notification by.
- 🧹 **The SMTP `retry` setting is not honoured and is no longer read.** The
  module always made exactly one attempt per submission; reading the value
  made it look otherwise. `retry`/`retries` are registered for every client
  module centrally, so they still appear in the SMTPClient reference, but
  `SMTPClient` does not act on them.

---

- 🔒 **NRPE hardening: a new `expose version` setting, and the metachar
  guard now also checks decoded input.** Nothing to do on a default install.
  Two things are worth knowing. `NRPEServer` gained `expose version` (default
  `true`, which keeps the legacy banner `check_nrpe` expects); set it to
  `false` to answer the unauthenticated `_NRPE_CHECK` ping with a generic
  message instead of the exact build. And `allow nasty characters = false` now
  re-checks the *decoded* command and arguments, not just the raw wire bytes —
  with a non-UTF-8 `encoding` set, a multi-byte sequence could previously
  decode into a metacharacter that was never literally on the wire, so a
  request the guard was always meant to block may now be rejected. Separately,
  `nscp nrpe install` reads the stored `verify mode` again instead of silently
  resetting it on a re-run.
  See the [security notice](../security/notices.md#nrpe-decoded-argument-metachar-guard-optional-version-banner-and-consistency-fixes).
- 🔒 **WEB server security hardening.** A review of the `WEBServer`
  module produced several defense-in-depth fixes (session tokens now come from
  the OpenSSL CSPRNG, cookie-name matching requires a name boundary, the
  installer refuses an HTTPS→HTTP redirect, and the `legacy` grant's startup
  warning now names `/settings/query.pb`). The default install needs no
  action. Two changes touch observable behaviour: a script or module **name
  that begins with `-` is now rejected** (rename it; interior dashes are
  fine), and the legacy **`POST /auth/logout` route now enforces `allowed
  hosts`** like the rest of the API (a caller outside the perimeter gets 403).
  A third only bites on a broken system: if the OpenSSL CSPRNG fails, the
  server now **refuses to issue a session token** (HTTP 500, logged as
  `SECURITY:`) rather than falling back to a weaker generator.
  See the [security notice](../security/notices.md#web-server-security-hardening).
- 🔒 **The bundled OpenSSL is updated from 3.5.4 to 3.5.8** in the Windows
  builds, picking up the fixes from four upstream security releases on the
  3.5 LTS line. The most relevant fix for NSClient++ is
  [CVE-2025-11187](https://nvd.nist.gov/vuln/detail/CVE-2025-11187), a stack
  buffer overflow parsing a crafted PKCS#12 file — reachable through
  `check_certificate` when it scans certificate files that less-trusted
  principals can write to — alongside further PKCS#12/ASN.1 and TLS-stack
  fixes. No configuration change is needed. The Linux packages link the
  distribution's OpenSSL and are unaffected. See
  [Security notices](../security/notices.md).

## 0.17.0

- 🏷️ **Check-specific filter keywords that clashed with the generic summary
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
- 🔢 **Check messages can now be told how to render their numbers.** Every filter
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
- 🔢 **An unknown unit in `format_bytes()` is now reported instead of rendering
  nonsense.** `format_bytes(used, 'gb')` used to render `1.27055e-10` because
  the unit comparison was case sensitive, and any misspelled unit rendered
  `value/1024^7`. Lowercase units now work, and a unit that names nothing (say
  `'ZB'`) makes the check report `Filter processing failed: format_bytes
  failed: Unknown byte unit: ZB`. A syntax string with such a typo returns
  UNKNOWN rather than a quietly wrong number - fix the unit, or the check will
  stay UNKNOWN.
- 📊 **A `unit:` in `perf-config` that names no unit no longer divides the metric
  by 1024⁷.** An unrecognised unit now leaves the value alone. If a graph of
  yours has been flat at a near-zero value, check the `unit:` spelling in its
  `perf-config`: the metric will jump to its real magnitude on upgrade.
- 📊 **`perf-config`'s `unit:` now converts plain byte series instead of
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
- 🧩 **Errors raised while a template renders are now reported.** A function that
  failed inside `detail-syntax` or `top-syntax` used to leave the placeholder
  empty and say nothing; the check now returns UNKNOWN with `Filter processing
  failed: …`. This surfaces template mistakes that have been silently producing
  incomplete messages.
- 🪟 **`check_pending_reboot`'s default message now names the pending-since
  time.** When the reboot was queued by Component Based Servicing or Windows
  Update, the message gains a suffix: `Reboot required: Windows Update` became
  `Reboot required: Windows Update (pending since 2026-08-16 09:41:12)`.
  Notification pipelines that match the exact message text (an anchored regex,
  a string equality) need their pattern relaxed; thresholds, states and
  existing keywords are unchanged.
- 🔢 **Filter comparisons between a text keyword and a bare number are now
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
- 🔢 **Fractional numbers in thresholds are no longer truncated or rounded.**
  `count > 2.5` used to evaluate as `count > 3` (the literal was rounded
  into the counter's integer domain); unit literals lost their fraction
  entirely, so `working_set > 1.5g` meant 1g and `uptime < 2.5h` meant 2h.
  Fractions now mean what they say. Whole-number thresholds are unchanged;
  only expressions that already used a decimal point can behave differently.
- 📊 **`filter_perf`/`render_perf`/`xform_perf`: the `max` and `min` filter
  keywords were swapped.** `max` read the perf-data *minimum* bound and
  `min` the *maximum*. They now read the bounds they name — a filter that
  compensated for the swap needs the two names exchanged back.
- 📨 **Syslog submission works again, so a configured syslog server will start
  receiving traffic.** `SyslogClient` read its connection settings from the
  wrong place, so the target's address, port, facility, severity and templates
  were all ignored: the agent logged `Undefined facility:` and sent nothing.
  Broken since 0.4.3 (2015). If you have a syslog target configured, check it
  still points where you want before upgrading - it has not been delivering,
  and it will now. `CheckMKClient` had the same defect on its query path.
- ✉️ **SMTP notifications now announce this host in EHLO instead of
  `localhost`.** The sender's host name was read from the wrong place, so it
  was always empty and the EHLO fell back to `localhost`. If your mail server
  applies HELO/EHLO policy (SPF checks, or a rule that rejects `localhost`),
  the agent will now identify itself properly - set `ehlo-hostname` on the
  target if you need a specific name.
- 🔧 **`nscp settings --show` now says so when `--key` is missing.** `--show
  --path /some/path` without a `--key` used to print nothing and exit 0; it
  now reports `Invalid command line please use --path and --key with show`
  and exits non-zero. A bare `--show` still describes the active
  settings store, and `--show --path … --key …` is unchanged. Scripts that
  relied on the silent success need the missing `--key` added.
- 💬 **Client commands shorter than eight characters work again.** A command
  such as `cpu` or `run` answered `Exception processing command line:
  basic_string::substr …` instead of running, in every module built on the
  shared client machinery (NRPE, NSCA, NRDP, Graphite, …). Remove any
  workaround that renamed such commands to a longer alias; no configuration
  change is needed.
- 🔧 **The settings diff no longer reports changes that were already saved.**
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

- 🔌 **`check_nt` (NSClientServer) answers the real nagios-plugins client again.**
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

- 🐧 **RHEL/SUSE:** workaround `ca=` arguments can be dropped — `${ca-path}` now
  resolves on its own (the explicit form still works). Packagers cross-building
  for another distribution should set `-DCONFIG_CA_PATH=`.
- 📄 **`check_logfile`** is unchanged unless you opt in to `bookmark` / `max-lines`.
  Adopting `bookmark` is a trade-off: a line is consumed when the check runs
  (not when its result is submitted) and positions are saved on clean shutdown,
  so a crash re-reports the backlog. Prefer an explicit bookmark name for a
  check whose filter changes often.
- 🔧 **Settings URLs with a query string now send it.** A server that relied on
  receiving the bare path will now see the parameters. The offline-boot cache
  file is migrated to the query-aware name once on first start.
- 🏷️ **`${hostname}` in an existing config changes meaning** — it is now expanded
  everywhere `expand_hostname` is used (including submit clients' `hostname`),
  where it used to be left as literal text.
- ⏱️ **`run on startup` is off by default** so the default install is unaffected;
  if you enable it for the `default` schedule use `startup window` to avoid a
  thundering herd.
- 📚 **Building the HTML docs on non-Windows** now needs `-DNSCP_BUILD_DOCS_HTML=ON`.

## 0.14.1

- ⚖️ **Licence change:** now distributed as **Apache-2.0 OR GPL-2.0-only** — a
  clarification/relicensing with no code or runtime behaviour change. Review it
  if your organisation tracks bundled-software licences.
- 📊 **CheckNet perfdata is on by default.** If you added perfdata manually,
  make sure you are not now emitting it twice.
- ☑️ **Boolean check arguments** (`option=true` / `option=false`) now work from the
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
- 📁 **`[/paths]` overrides** from an older install moved to `[paths]` in
  `boot.ini` (same section name, different file). No automatic migration — copy
  each entry across and delete the old section.
- 🔒 **WEB `disable admin user = true`** is a new opt-in for status-only WEB
  exposure; existing installs keep their admin unchanged.
- 🔌 **NRPEServer** now survives a failed listener (logs an ERROR, leaves the
  module loaded) instead of failing the whole module. Add "NRPE listener failed"
  as a signal if you alerted on module-load failure.
- 🔊 **`insecure = true` on NRPEServer** now logs at ERROR (louder, behaviour
  unchanged) — whitelist the message on agents intentionally run insecure.

## 0.12.5

- 📁 **`[/paths]` users:** copy entries into `[paths]` in `boot.ini`; the
  settings-side section is no longer consulted. Default installs are unaffected.
- 🧩 **Custom-plugin authors:** implement the new optional `prepare_shutdown`
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

- ✅ No configuration migration required (new `proxy` keys are opt-in). The
  `check_files` fixes change a few corner cases: `max-depth=0` now scans the top
  directory (#730); missing paths return UNKNOWN (#613); junction loops are not
  double-counted (#605); empty results return OK instead of UNKNOWN (#717).
  Review alerting that relied on the old corner-case behaviour.
