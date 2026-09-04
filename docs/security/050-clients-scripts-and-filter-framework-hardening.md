---
title: "Security hardening across the clients, scripts and filter framework"
fixed_in: 0.18.1
severity: "High for NSCA-NG cert-mode targets,"
modules: [IcingaClient, NRDPClient, NSCAClient, NSCAServer, NSCANgClient, CheckMKClient, ElasticClient, GraphiteClient, SyslogClient, SMTPClient, CheckExternalScripts, filters]
---
Low–Medium for everything else

One sweep over the outbound client modules (Icinga, NRDP, NSCA, NSCA-NG,
check_mk, Elastic, Graphite, syslog and a second round on SMTP), the
external-script launcher and the filter framework. Three themes run through it:
**configuration that silently did not apply**, **operations that could never
time out**, and
**attacker-influenced text reaching another system's log unscrubbed**. None of
it is remotely exploitable code execution on a default install; the NSCA-NG and
check_mk items are the ones that change what goes on the wire.

#### Settings that were read but never applied

- **NSCA-NG cert mode ignored its TLS configuration** (High; affects 0.18.0,
  `use psk = false` only). The OpenSSL context was configured *after* the TLS
  stream had been created from it, and `SSL_new()` copies the verify mode,
  certificate, cipher list and version bounds out of the context at creation
  time — so `verify mode = peer-cert` ran with verification off and accepted
  any certificate, a man-in-the-middle's included, and the configured client
  certificate was never presented. The default PSK mode authenticates both
  ends through the pre-shared key and was never affected.
- **check_mk client TLS keys were discarded** (Medium). The target object
  never called `register_all()`/`notify()`, so `use ssl`, `certificate`,
  `certificate key`, `ca`, `allowed ciphers`, `verify mode` and `dh` were
  parsed and thrown away — the client connected in plaintext whatever the
  configuration said, and the keys were missing from the reference docs.
- **An unrecognized NSCA `encryption` value meant "no encryption".** A typo
  (`aes-256`), or an algorithm not compiled into the build, silently resolved
  to plaintext on the end carrying it. It is now a hard error naming the
  available algorithms: the `NSCAServer` module refuses to load and an
  `NSCAClient` submission fails. `encryption = none` remains the explicit way
  to run unencrypted.
- **Syslog severities and templates did nothing.** The per-state severity
  options (`ok-severity`, …) and the `tag_syntax` / `message_syntax` target
  settings were stored under keys the sender never read, so they parsed fine
  and silently did nothing — leaving submissions on the emergency fallback
  below, and settings-defined targets sending an empty tag.
- **`ext-scr install --arguments=…` wrote to a path nothing reads**, so an
  argument lockdown reported success while leaving arguments enabled.
- **NSCA server `performance data = false` was ignored**; perfdata is stripped
  from forwarded submissions again, as documented.

#### Operations that could never time out

Each of these ran with no deadline, so an endpoint that accepted the connection
and then stalled held the submitting thread indefinitely — silently stopping
passive results until a service restart:

- **Icinga** API calls, **NRDP** submissions, **Graphite** carbon submissions
  and the recurring metrics flush, and **Elastic** bulk submissions. Each is
  now bounded by the target's configured `timeout` (default 30 s) as a single
  budget covering name resolution, connect, TLS handshake and the exchange.
  NRDP additionally retries transport failures up to `retry`, each attempt on
  a fresh connection; Graphite's `retry` never had any effect and is gone.
  Several of these settings were also looked up in the free-form option map
  while the framework routes them into typed fields, so a configured value was
  ignored even where a timeout *was* honoured.
- **External scripts.** On Unix the single-string shell fallback ran through
  `popen()`, which hides the child PID, so `timeout=` was silently unenforced
  and a hung script wedged a worker thread per invocation. On Windows the read
  loop counted iterations rather than elapsed time, so a continuously chatty
  script escaped the timeout entirely and leaked an unkillable process each
  run. Both launchers now bound the wait by wall-clock deadline.
- **SMTP** did bound its submission, but a budget that expired mid-connect left
  the cancelled operation's completion handler queued, to be run by the retry
  against the next resolved address with references into a stack frame that no
  longer existed — a use-after-return in a long-running service. Handler state
  is heap-owned now, and a spent budget ends the endpoint walk instead of
  retrying into it.

#### Injection, scrubbing and resource limits

- **Graphite status paths** now go through the same scrub as the perf path.
  The `${check_alias}` substituted into them can originate from a remote
  submitter, so an alias carrying a newline injected an extra, attacker-chosen
  metric line into Graphite (and a `;` injected carbon tags) — a way to hide a
  real problem or fabricate one.
- **Syslog** emits the RFC 3164 HOSTNAME field. Without it a conforming
  receiver promoted the tag to origin host, so with a tag template expanding
  `%message%` check output chose which host a record was filed under. Every C0
  control byte and DEL is neutralised rather than just CR/LF/NUL, and an
  unknown `severity` or `facility` name falls back to `<13>` (user.notice)
  instead of `<0>` — kernel.emergency, which many receivers page or `wall` on.
- **Inbound NSCA wire fields** are validated before they reach logs and the
  inbox channel: control characters are stripped from the host and service
  names (a log-injection vector) and a return code outside 0–3 is clamped to
  UNKNOWN instead of flowing on as an arbitrary 16-bit integer.
- **Filter expressions** are capped at **1024 characters** and **64** nesting
  levels. Both the recursive-descent parser and the AST evaluator recurse with
  the shape of the input, so a long or deeply nested expression could exhaust
  the thread stack and crash the whole agent — reachable by anyone able to
  influence a filter string over the authenticated REST API, or over NRPE with
  `allow arguments = true`. The limits sit an order of magnitude above any real
  filter, and string literals are exempt from the depth count.
- **Buffers are bounded**: an NRDP response body is capped at 5 MB (previously
  unbounded, so a hostile server — or a man-in-the-middle on a plain `http://`
  target — could stream the agent out of memory), captured script output at
  8 MiB, and an SMTP reply at 64 KB per line and 100 lines — nothing capped
  how much a peer could make the client buffer inside its timeout window, so
  bytes without a line ending, or endless `250-` continuations, turned a
  30-second budget into gigabytes of agent memory.
- **SMTP reply text is rendered inert before it reaches the log.** Error
  messages quoted server replies verbatim into the agent log and the submit
  response. Anything outside printable US-ASCII is replaced now — the C0
  controls and the C1 range (0x80–0x9F), which carries single-byte terminal
  escapes such as CSI — so a multi-line reply can no longer forge extra log
  lines, and an oversized one is truncated. Two smaller items came with it:
  the reply to `STARTTLS` must be exactly `220` per RFC 3207 rather than any
  `2xx`, and AUTH credentials containing a NUL are refused before connecting
  (a NUL shifts the RFC 4616 `AUTH PLAIN` field boundaries).
- **The `ext-scr show` / `delete` sandbox resolves symlinks** before its
  containment test; previously a symlink inside the script root pointing
  outside it let an authenticated admin read or remove files anywhere the
  service account could reach. On the Windows shell fallback `%` and `^` are
  now refused as well (cmd.exe `%VAR%` expansion and its escape character),
  opt-out via `allow nasty characters`.

#### TLS and credentials

- **Elastic submissions verify the server certificate.** Verification was
  hardcoded to `none`, so an `https://` address validated neither the chain
  nor the host name while shipping event-log entries, metrics and the agent
  log. It defaults to `peer` against the platform CA bundle now, with
  `tls version`, `verify mode` and `ca` settings matching the other HTTP
  clients, and new `user`/`password` and `api key` authentication —
  Elasticsearch has enabled security by default since 8.0, which the module
  previously could not satisfy at all.
- **`tls version = 1.2+` means "1.2 or later" again.** The `+` was stripped and
  the value mapped onto a version-pinned OpenSSL method, which pins the
  *maximum* too, so the widely used default negotiated TLS 1.2 only and
  silently excluded 1.3; `any` is accepted as the documentation always claimed.
  The fix is in the shared stack: all HTTP-based clients, the NRPE/NSCA socket
  clients and servers, and `check_tcp`.
- **Credentials are masked in the trace log.** At log level `trace` the whole
  target configuration was dumped, printing the raw `password` / `token`
  values; the fix sits in the shared client machinery, so every outbound
  client module is covered.
- **Unverified links are called out.** An Icinga `https` submission whose
  `verify mode` resolves to no peer verification logs a message naming the
  endpoint, since the API credentials then go to whichever server answers. An
  empty NSCA `password` with encryption enabled logs an error too: the key is
  the password zero-padded with no derivation step, so an empty one is a
  well-known all-zero key that anyone past `allowed hosts` can forge with.

**What to do:** most installs need nothing. Specifically:

- **NSCA-NG cert mode:** upgrade if any target sets `use psk = false`. A target
  whose server certificate does not chain to the configured `ca` (or does not
  match the host name) will now fail to connect — that is the verification
  taking effect; fix the certificate, or accept the exposure explicitly with
  `insecure = true`.
- **check_mk:** a target carrying `use ssl = true` now really negotiates TLS,
  so a plaintext-only server end will start failing — loudly, which is the
  point.
- **Elastic over `https` with a self-signed certificate:** point `ca` at it, or
  set `verify mode = none` to keep the old insecure behaviour. On Elasticsearch
  6.x or older set `event type`, `metrics type` and `nsclient log type`
  explicitly — the legacy `_type` parameter is no longer sent by default.
- **NSCA:** fix any typo'd `encryption` value (that end was silently running
  plaintext, usually visible as the peer rejecting submissions with a CRC
  error), and set the same real `password` on both ends if the new
  empty-password error appears. If you relied on `performance data = false`
  while it was broken, perfdata really is dropped now.
- **External scripts:** re-run `ext-scr install` after upgrading so an argument
  lockdown lands on the setting the module actually reads. Treat write access
  to any `script path` directory, and the ability to configure external-script
  commands, as equivalent to code execution as the service account.
- **Slow endpoints:** a submission to an unresponsive Icinga, NRDP, Graphite or
  Elastic endpoint now fails after `timeout` instead of hanging — raise it on
  that target if the endpoint is legitimately slower. Peers negotiating with a
  `+` or `any` TLS version can now select TLS 1.3; pin an exact version if one
  misbehaves on it.
- **Syslog receivers** whose parsing rules keyed on the old malformed datagram
  (no HOSTNAME field) need adjusting — records now arrive attributed to the
  agent's host name instead of the tag. Syslog remains cleartext and
  unauthenticated: keep the path to the server on a trusted segment.
