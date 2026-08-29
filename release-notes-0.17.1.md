# Passive checks that actually arrive, and four security reviews

0.17.1 is a correctness and hardening release. Three things that reported
success while doing nothing are fixed: `check_and_forward` built its submission
in a form no channel could read, `check_nscp` had counted zero crash reports
since 0.4.2, and a module enabled by a fleet bundle was never loaded until the
service restarted. Alongside that, security reviews of the SMTP, NRPE, NRDP/NSCA
and WEB modules landed, the bundled OpenSSL moves to 3.5.8, and `run_schedules`
lets you submit a passive result without waiting out the interval.

## ✨ Highlights

- 📤 **`check_and_forward` submits again.** The command ran the check, answered
  `Message submitted` and delivered nothing: the submission was built as a query
  message, which no channel can read. NSCA, NRDP, Graphite and every other
  client module were equally affected. It also gained `channel`, `alias`,
  `destination` and `source`. (#1452)
- 📅 **New: `run_schedules`.** Run the configured schedules now instead of waiting
  out their interval, and submit the results on their normal channel —
  `nscp client --boot --query run_schedules`, optionally
  `--argument schedule=<alias>`. Works over NRPE, REST and `nscp test` too.
  (#1450, #1452)
- 💥 **`check_nscp` is a filter check, and its crash count works.** It has read 0
  crashes since 0.4.2 (it matched the extension `txt` against `.txt`), and since
  0.6.10 there were no `.txt` reports to find. It now recognises `.crash`, reads
  the configured `archive folder` again, and exposes `crashes`, `errors`,
  `uptime`, `crash_age`, `last_crash`, `last_error`, `version` and `date` as
  filter keywords. (#1451)
- ⚙️ **A reload now loads modules enabled in an included file.** A fleet bundle
  that switched on `NRDPClient` and `Scheduler` applied cleanly, reported the
  host in sync, and submitted nothing — the include was still the copy cached at
  load time, so the modules were only really loaded at the next restart. (#1455)
- 🛡️ **Four security reviews** — SMTP, NRPE, NRDP/NSCA and WEB — closed a set of
  defense-in-depth gaps: STARTTLS response injection, an unvalidated EHLO name,
  certificate verification that could not work on Windows, a metachar guard that
  ran before decoding, secrets in the trace log, and session tokens from a
  non-cryptographic generator.
- 🔐 **The bundled OpenSSL moves from 3.5.4 to 3.5.8** in the Windows builds,
  picking up four upstream security releases — most relevantly CVE-2025-11187,
  a stack overflow parsing a hostile PKCS#12 file, reachable through
  `check_certificate`. (#1445)
- 🪟 **The MSI can install your own TLS certificates.** `CERTIFICATE`,
  `CERTIFICATE_KEY` and `CERTIFICATE_CA` place your files where every server
  module reads them, so the self-signed fallback is never generated. (#568)
- ⏱️ **Real-time filters can prime their destination at startup.** A new
  `run on startup` key submits the filter's empty message once when the agent
  starts, so `check_cache` stops answering "Entry not found" after a restart.
  (#584)
- 📡 **New `${address_ipv4}` / `${address_ipv6*}` hostname placeholders** for every
  passive client, so a host can report itself by address instead of name. (#349)

## 🔍 Detailed changes

### 📤 CheckHelpers — `check_and_forward` delivers, and takes arguments properly

The command handed the raw `QueryResponseMessage` to the submission path, but
channels parse a `SubmitRequestMessage` and the two are not wire compatible —
the payload sits in a different field, and field 2 of a submit message is the
channel string. The channel received a message with zero payloads and cheerfully
reported success. It now converts the query result into a submission first and
checks the reply.

| Option | Meaning |
|--------|---------|
| `channel` | Where to submit (default `NSCA`); `target` remains a synonym |
| `alias` | Service description; defaults to the wrapped command's name |
| `destination` | Destination host for the submission |
| `source` | Source host for the submission |

`target` previously defaulted to the empty string, for which no handler exists,
so even a correctly built message had nowhere to go.

### 📅 Scheduler — `run_schedules`

`run_schedules` executes the schedules under `[/settings/scheduler/schedules]`
immediately and submits each result on its own channel, target, source and alias
with the same report filter, so the monitoring server cannot tell it from a
timed run. The timers are untouched.

```
nscp client --boot --query run_schedules
nscp client --boot --query run_schedules --argument schedule=cpu
```

`schedule=` is repeatable and defaults to every schedule; an unknown alias is an
error that names the ones you have. A schedule whose command is `run_schedules`
is refused, and a reentrancy guard catches the indirect case (via
`check_timeout`, for instance) — previously that recursed until the agent died.

The caller's identity is forwarded to the checks it runs, so REST and NRPE
permissions apply to them; the scheduler's own timed runs stay attributed to
`Scheduler`.

### 💥 CheckNSCP — `check_nscp` rewritten as a filter check

Three independent bugs kept the crash count at zero: the extension comparison
(`txt` vs the `.txt` the helper returns) has been false since 0.4.2; 0.6.10
replaced breakpad's `<guid>.dmp` + `.dmp.txt` pair with a single
`<timestamp>.crash` file, so even a fixed match found nothing; and 0.4.3 stopped
reading `[/settings/crash]` `archive folder`, hardcoding the compile-time
default. `last_crash` was never populated either — the newest-file watermark
started at the current time.

New filter keywords: `crashes`, `errors`, `uptime`, `crash_age`, `last_crash`,
`last_error`, `version`, `date`. Thresholds accept duration units
(`crit=uptime < 5m`, `crit=crash_age < 7d`), and a new `max-unit` option
(default `w`) caps the largest unit rendered. Crash reports are a Windows
concept; on Linux `crashes` is always 0.

### ⚙️ Core — reloads, channel verdicts and denied checks

- **A reload re-reads the included files** before deciding which modules should
  run, so a module enabled in one since the last load is picked up. Only the
  includes are refreshed: clearing the whole store would discard configuration
  held in memory, which is exactly how `nscp unit` and `nscp client` set
  themselves up. One unreadable include no longer aborts the whole reload.
  (#1455)
- **Every channel's verdict is reported** for a channel list. All handlers were
  handed the same response buffer, so with `channel=NSCA,GRAPHITE` a failing
  NSCA was masked by a succeeding GRAPHITE.
- **A denied check is no longer submitted.** The permission layer answers a
  denied query as a *successful* query carrying an UNKNOWN "Permission denied"
  payload; `run_schedules` and `check_and_forward` forwarded it, overwriting the
  last real result on the server while reporting success to the caller.
- `nscp client --query <cmd>` no longer appends `No module was specified…` to
  every result.

### 🛡️ Security reviews

🔒 **SMTPClient.** Data pipelined across the STARTTLS handshake is refused
(RFC 3207 §4) — a prepared run of 2xx replies could otherwise walk the client
through MAIL/RCPT/DATA and have it report an alert delivered while nothing was
sent. The EHLO name is validated before connect, closing command injection on a
relayed submission. Certificates are verified against a CA bundle through a new
`ca` target setting and `--ca` argument (default `${ca-path}`): the client
previously used OpenSSL's default verify paths only, which on Windows excludes
the certificate store, so `security=starttls` failed against Gmail and M365 and
operators simply turned verification off. EHLO capabilities are matched per
reply line rather than by substring — a greeting naming host
`starttls.example.com` used to satisfy the STARTTLS lookup.

🔒 **NRPE.** The `allow nasty characters = false` guard now also runs on the
*decoded* command and arguments; with a non-UTF-8 `encoding`, a multi-byte
sequence could decode into a metacharacter that was never literally on the wire.
A new `expose version` server setting (default `true`) lets the unauthenticated
`_NRPE_CHECK` reply stop naming the exact build. `nscp nrpe install` reads the
stored `verify mode` again instead of silently resetting it on every re-run.

🔒 **NRDP / NSCA.** A malformed `<status></status>` response no longer
null-derefs the agent. The NRDP token and any proxy-URL credentials are redacted
from the trace log — including from the `Target configuration:` dump, which
printed the raw settings map and defeated the redaction elsewhere (NSCA's
`password` leaked the same way). An `https://` submission made through
`nscp client` or REST with no `verify mode` now defaults to `peer` rather than
trusting any certificate.

🔒 **WEBServer.** Session tokens and generated admin passwords come from
OpenSSL's CSPRNG with unbiased rejection sampling, and the server now fails
closed if that RNG fails (HTTP 500 and a `SECURITY:` log line) rather than
falling back to a weaker generator. Cookie lookups require a name boundary, so
`eviltoken` no longer satisfies a lookup for `token`. Session validity and
identity are read in one locked observation, closing an expiry race that could
drop a request onto the anonymous grant. The web installer refuses an
HTTPS→HTTP redirect on the bundle download path.

🔒 **OpenSSL 3.5.8.** Windows builds only; Linux packages link the
distribution's OpenSSL. See
[Security notices](https://nsclient.org/docs/security/notices/).

### 📡 Clients — `--source-host` names the sender

`--source-host` / `--sender-host` were registered against the *destination*
container, where the well-known `host` key is routed into the typed address
field — so naming a source host silently redirected the connection to it, and
the sender the handler reads was never set. `SMTPClient` and `NRDPClient` had
each worked around this with their own copies, which made the option ambiguous
and therefore unusable on exactly the two modules most likely to need it.

### 🔧 Settings

- `settings --update --add-defaults --use-samples` writes the sample objects; the
  flag was parsed and never read. `--remove-defaults` now enumerates samples too,
  making the two exact inverses. (#233)
- A `[/includes]` entry naming a directory no longer breaks saving. The directory
  path reached the INI writer, and the resulting "Is a directory" error aborted
  the save — so `nscp settings --set` and the web UI failed *and the main file
  was never written*. (#636)
- New `${address_ipv4}` and `${address_ipv6*}` placeholders resolve in the
  `hostname` setting of NSCA, NSCANg, NRDP, Graphite, Syslog, Op5, Icinga,
  Elastic and Collectd clients. The address is the source address of the default
  route, falling back to the first non-loopback address the host name resolves
  to. (#349)

### ⏱️ Real-time filters — `run on startup`

A new boolean filter key on the shared filter object, so it applies to
CheckLogFile, CheckEventLog and the CheckSystem/CheckSystemUnix real-time
filters. When true the filter submits its `empty message` with OK status once at
startup, priming the destination. It is registered without a default on purpose:
an absent key keeps the inherited value, while an explicit `false` overrides an
inherited `true`. Delivery is retried across shortened waits while later modules
(such as SimpleCache) are still loading. (#584)

### 🪟 Windows installer — install your own TLS certificates

Three new silent-install properties place your own files under the default names
every server module reads, so the self-signed fallback is never generated:

| Property | Installed as |
|----------|--------------|
| `CERTIFICATE` | `certificate.pem` |
| `CERTIFICATE_KEY` | `certificate_key.pem` |
| `CERTIFICATE_CA` | `ca.pem` |

The install fails if a named file is missing or is not PEM, or if a certificate
is given with no key anywhere. `CERTIFICATE_KEY` also writes
`certificate key = …` for the NRPE and WEB servers, so it is incompatible with
`ALLOW_CONFIGURATION=0`; other servers need the setting added by hand. (#568)

### 🐛 Bug fixes

- SMTP: the timeout error now says what happened in the client's own words
  (`timed out after 30s (the budget for the whole submission)`) rather than
  reporting a platform error that blamed the connected party; a failed connect
  raises the exception callers actually catch; `insecure-skip-verify` is
  accepted over REST and no longer resets a configured target's value on every
  submission; the reference no longer shows its default as `N/A`.
- NRPE: the arguments-case rejection says "arguments" instead of "command".
- `check_nscp`: a crash report whose timestamp cannot be read still counts but
  takes no part in newest-wins, so `crash_age` no longer reports ~56 years.
- Configuring with `-DNSCP_BUILD_TESTS=OFF` no longer aborts on the
  `mongoose_wrapper_test` target.

### 📦 Packaging

The WinGet publish workflow syncs the bot's `winget-pkgs` fork before submitting.
A fork left alone between releases drifts far enough that `wingetcreate`'s
internal fast-forward fails — which is exactly how the 0.17.0 WinGet submission
failed, reporting only "The forked repository could not be synced with the
upstream commits." Failures now name the fork, the HTTP status and the way out.

### 🧪 Testing

New suites cover the SMTP wire conversation against a scripted server, the
`check_nscp` keywords with planted crash reports, `run_schedules`, permission
forwarding, `check_and_forward` submission, schedules loaded from an include,
and the `--add-defaults` contract. The web UI gained the test suites it did not
have: 65 unit tests and 18 browser tests against the production bundle.

## ⚠️ Upgrade notes

- **`check_and_forward` starts delivering, and no longer takes positional
  arguments.** Anything that treated `Message submitted` as success will now see
  real channel failures. `check_and_forward command=check_cpu warn=load>80` now
  fails to parse — pass one `arguments=` per wrapped argument instead:
  `check_and_forward command=check_cpu "arguments=warn=load>80"`. The positional
  form had to go because its parser also swallowed the CLI's own
  `--argument key=value` tokens, which is what fed the wrapped command garbage.
- **`check_nscp` may start reporting CRITICAL.** Its crash count works now, so an
  agent with an old report still in the archive folder will report a crash where
  it read 0. Threshold on `crash_age` (`"crit=crash_age < 7d"`) if you only care
  about recent crashes, or clean the folder out. The message also loses its
  `last crash:` / `last error:` fragments — put them back with an explicit
  `detail-syntax` if you match on the text.
- **A denied check now fails instead of submitting.** If you restrict what a REST
  or NRPE identity may run, expect an error where a stale UNKNOWN previously
  appeared on the monitoring server.
- **NRDP over HTTPS verifies by default** on the `nscp client` / REST path. Pass
  `--verify none` (or point `--ca` at the certificate) to keep submitting to a
  self-signed endpoint that way. Configured targets already defaulted to `peer`.
  🔒
- **SMTP verifies the server certificate**, against `${ca-path}` by default. A
  target that relied on verification being effectively off needs `ca` pointed at
  the right bundle, `ca = none` for OpenSSL's defaults, or
  `insecure-skip-verify = true`. 🔒
- **The SMTP `timeout` is now a budget for the whole submission**, not a fresh
  deadline per operation — a target that only completed by consuming several
  multiples of it will now give up.
- **`--source-host` no longer redirects the connection.** If you used it to
  choose where to connect, use `--host` / `--address`.
- **`settings --add-defaults --use-samples` now writes samples.** Drop the flag
  if you were relying on today's sample-free output; `--remove-defaults` strips
  them again.
- **`nscp client --query` output loses its trailing `No module was specified…`
  line.** Scripts that stripped it can stop.
- **Settings writes work again when `[/includes]` names a directory.** If you
  removed a directory include to get saving working, you can put it back.
- **WEBServer:** a script or module name beginning with `-` is rejected (rename
  it; interior dashes are fine), and `POST /auth/logout` now enforces
  `allowed hosts`. 🔒
- **NRPE:** `allow nasty characters = false` now also inspects decoded input, so
  a request the guard was always meant to block may now be rejected. Set
  `expose version = false` to stop the `_NRPE_CHECK` ping naming your build. 🔒
- **A fleet bundle can now enable a module without a restart** — the reload picks
  up modules switched on in an included file. Unchanged: a module *disabled*
  since the last load keeps running until the service restarts, and `[/modules]`
  in the main `nsclient.ini` is still only read at startup.

**Full Changelog**: https://github.com/mickem/nscp/compare/0.17.0...0.17.1
