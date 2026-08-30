# Security notices

This page is the running record of security-relevant changes in NSClient++ —
both published advisories (with their CVE / GHSA identifiers) and security
**hardening** that changed behaviour but was not assigned a CVE. It complements
the GitHub [Security Advisories](https://github.com/mickem/nscp/security/advisories)
tab, which lists only *published* advisories and so does not capture hardening
changes.

- To **report** a vulnerability, follow the
  [security policy](https://github.com/mickem/nscp/blob/main/SECURITY.md).
- For the concrete operator actions a given release requires, see
  [Upgrading](../setup/upgrading.md).

---

## Published advisories

| Advisory | CVE | Severity | Affected | Summary |
|---|---|---|---|---|
| Upstream ([OpenSSL](https://github.com/openssl/openssl)) | [CVE-2025-11187](https://nvd.nist.gov/vuln/detail/CVE-2025-11187) and the other OpenSSL 3.5.5–3.5.8 fixes | Moderate (highest reachable) | Windows builds up to 0.17.0 (bundled OpenSSL ≤ 3.5.4) | Crafted-input flaws in the bundled OpenSSL; the most severe reachable one is a stack overflow parsing a hostile PKCS#12 file via `check_certificate`. |
| Upstream ([Cesanta Mongoose](https://github.com/cesanta/mongoose)) | [CVE-2026-73256](https://nvd.nist.gov/vuln/detail/CVE-2026-73256), [CVE-2026-73257](https://nvd.nist.gov/vuln/detail/CVE-2026-73257) | Critical (9.1) | Windows builds up to 0.16.3 (bundled Mongoose < 7.22) | HTTP request smuggling in the bundled Mongoose web server; exploitable behind a reverse proxy / WAF. |
| [GHSA-rhrw-79v5-jc5x](https://github.com/mickem/nscp/security/advisories/GHSA-rhrw-79v5-jc5x) | CVE-2025-34079 | High (7.8) | 0.5.2.35 and earlier | Authenticated remote code execution via `ExternalScripts`. |
| [GHSA-jr25-22p3-gm6r](https://github.com/mickem/nscp/security/advisories/GHSA-jr25-22p3-gm6r) | CVE-2025-34078 | High (7.8) | 0.5.2.35 and earlier | Local privilege escalation from plaintext credentials in the configuration file. |

### OpenSSL 3.5.5–3.5.8 — bundled OpenSSL updated past four upstream security releases

**Fixed in:** 0.18.0 (bundles OpenSSL 3.5.8) · **Severity:** Moderate (highest fix reachable through NSClient++)

The **Windows builds** of NSClient++ statically bundle
[OpenSSL](https://github.com/openssl/openssl) as the TLS engine for the
NRPE/NSCA/NSClient server and client modules and the web server / REST API,
for certificate parsing in `check_certificate`, and for password hashing.
Releases up to 0.17.0 bundled OpenSSL 3.5.4; the four upstream security
releases since then — 3.5.5 (27 Jan 2026), 3.5.6 (7 Apr 2026), 3.5.7
(9 Jun 2026) and 3.5.8 (25 Aug 2026) — fix some forty CVEs on the 3.5 LTS
line, and 0.18.0 moves the bundled copy to 3.5.8.

The fixes most relevant to NSClient++ sit on code paths it actually calls:

- **[CVE-2025-11187](https://nvd.nist.gov/vuln/detail/CVE-2025-11187)**
  (Moderate) — missing validation of PBMAC1 parameters in PKCS#12 files can
  trigger a stack buffer overflow or invalid pointer dereference during MAC
  verification. `check_certificate` parses operator-specified certificate
  files and falls back to PKCS#12 (`.pfx`/`.p12`) parsing, so a hostile
  certificate file scanned by a check reaches this code.
- Several further low-severity fixes on the same PKCS#12/ASN.1 parsing paths
  ([CVE-2025-69421](https://nvd.nist.gov/vuln/detail/CVE-2025-69421),
  [CVE-2026-22795](https://nvd.nist.gov/vuln/detail/CVE-2026-22795),
  [CVE-2026-34181](https://nvd.nist.gov/vuln/detail/CVE-2026-34181),
  [CVE-2026-7383](https://nvd.nist.gov/vuln/detail/CVE-2026-7383),
  [CVE-2026-34180](https://nvd.nist.gov/vuln/detail/CVE-2026-34180)) and in
  the TLS stack
  ([CVE-2025-66199](https://nvd.nist.gov/vuln/detail/CVE-2025-66199)
  TLS 1.3 `CompressedCertificate` memory growth,
  [CVE-2025-15468](https://nvd.nist.gov/vuln/detail/CVE-2025-15468)).

The highest-severity upstream fixes in this span —
[CVE-2026-45447](https://nvd.nist.gov/vuln/detail/CVE-2026-45447) (High,
use-after-free verifying a PKCS#7/S/MIME signature) and
[CVE-2025-15467](https://nvd.nist.gov/vuln/detail/CVE-2025-15467) (High,
stack overflow parsing CMS `AuthEnvelopedData`) — are in CMS / S/MIME code
NSClient++ never calls, and the QUIC, CMP and DTLS fixes likewise do not
apply.

Not affected: the Linux packages (DEB/RPM) link the distribution's OpenSSL
and receive these fixes through ordinary OS updates.

**What to do:** upgrade Windows installs to 0.18.0 or later — promptly if
`check_certificate` is pointed at certificate files or directories that
less-trusted principals can write to. No configuration change is needed.

### CVE-2026-73256 / CVE-2026-73257 — HTTP request smuggling in the bundled Mongoose web server

**Fixed in:** 0.16.4 (bundles Mongoose 7.23) · **Severity:** Critical (CVSS 9.1, upstream)

NSClient++ bundles the [Cesanta Mongoose](https://github.com/cesanta/mongoose)
embedded web server as the HTTP engine for the `WEBServer` module (the REST API
and web UI) in the **Windows builds** (`NSCP_WEB_BACKEND=mongoose`, the Windows
default). Two request-smuggling vulnerabilities were published upstream against
Mongoose's HTTP parser, both fixed in Mongoose 7.22:

- **CVE-2026-73256** — a broken HTTP/1.0 detection check in `http_cb()` lets a
  request that combines `Transfer-Encoding: chunked` with conflicting HTTP/1.0
  framing be parsed with different message boundaries than an HTTP/1.0 reverse
  proxy in front of it.
- **CVE-2026-73257** — a request carrying **both** `Content-Length` and
  `Transfer-Encoding: chunked` is accepted instead of rejected, enabling a
  classic CL.TE desynchronization against a Content-Length-preferring front end.

Both are only exploitable when requests reach NSClient++ **through an
intermediary** (reverse proxy, WAF, load balancer) that frames the request
stream differently than Mongoose: an unauthenticated attacker can then smuggle
a second request past the intermediary's access controls (path/method ACLs,
IP restrictions, proxy-level authentication) or inject it into another client's
reused connection. In the common direct client → NSClient++ deployment there is
no front end to desynchronize. NSClient++'s own authentication and WEB
permissions are evaluated per request inside the module, so a smuggled request
still needs valid NSClient++ credentials — what smuggling defeats is any
security control enforced *in front of* NSClient++.

Not affected: the Linux packages (DEB/RPM) build the `WEBServer` module on the
Boost.Beast backend and do not contain Mongoose at all; neither do builds with
the `WEBServer` module disabled.

**What to do:** upgrade Windows installs to 0.16.4 or later, which bundles
Mongoose 7.23 — promptly if NSClient++ sits behind a reverse proxy or WAF. As a
stop-gap, an HTTP/1.1-only front end that itself rejects requests with both
`Content-Length` and `Transfer-Encoding` reduces (but does not remove) the
exposure.

### CVE-2025-34079 — Authenticated RCE via ExternalScripts

An attacker who already holds valid administrator credentials can register and
run an external script, achieving remote code execution. It affects the legacy
`0.5.2.35` architecture that predates the current REST v2 API.

**What to do:** upgrade to a current supported release, and restrict who can
configure `ExternalScripts` / hold an administrator role. See the
[external scripts](../scenarios/external-scripts.md) and
[permissions](../concepts/permissions.md) guidance. Full detail:
[GHSA-rhrw-79v5-jc5x](https://github.com/mickem/nscp/security/advisories/GHSA-rhrw-79v5-jc5x).

### CVE-2025-34078 — Local privilege escalation via plaintext credentials

A principal with local filesystem access can read plaintext credentials stored
in `nsclient.ini` and reuse them. It affects the legacy `0.5.2.35` architecture.

**What to do:** upgrade to a current supported release; lock down the
configuration file with filesystem ACLs and, where possible, move sensitive
values into the credential manager (see
[Securing NSClient++](../setup/securing.md)). Full detail:
[GHSA-jr25-22p3-gm6r](https://github.com/mickem/nscp/security/advisories/GHSA-jr25-22p3-gm6r).

---

## Hardening changes (no CVE)

Security-relevant changes that are handled as defense-in-depth / consistency
hardening rather than assigned a CVE are listed here as they ship, newest
first, alongside the release that contains them.

### Icinga client security-review hardening

**Fixed in:** unreleased · **Severity:** Low

A security review of the `IcingaClient` module (and of the WEB server path
Icinga's `check_nscp_api` plugin authenticates through) confirmed the
defaults are sound — TLS verification on by default for configured and ad-hoc
targets alike, Icinga filter-expression escaping, and the User-Agent-gated
legacy auth — and produced three hardening fixes on the client side:

- **Credentials are redacted from the trace log.** With log level `trace`, the
  client logged the whole target configuration through
  `destination_container::to_string()`, which printed the raw data map —
  including the Icinga API `password`. The value of any `password` or `token`
  key is now masked in that output. The fix sits in the shared client
  machinery, so the other outbound client modules built on the same type
  (NRPE, NSCA, NRDP, …) are covered by it as well.
- **The configured `timeout` is enforced on Icinga API calls.** The HTTP
  client was built without a deadline (wait forever), so the target's
  `timeout` setting (default 30 s) was read but never applied — an Icinga
  endpoint that accepted the connection and then stalled could wedge the
  submitting thread indefinitely, silently stopping scheduler-driven passive
  results. This is an availability fix; set `timeout = 0` on the target if you
  really want the old unbounded wait.
- **Disabled certificate verification is called out.** An `https` submission
  whose `verify mode` resolves to no peer verification (empty, `none`, or
  `fail-if-no-cert` alone) now logs a message naming the endpoint, since the
  API credentials are then sent to whichever server answers. The option help
  also no longer suggests `none` for self-signed certificates — the right
  configuration is `peer-cert` with `ca` pointing at the certificate.

### Graphite client — status path scrubbed against metric-line injection

**Fixed in:** unreleased (first release after 0.18.0) · **Severity:** Low

`GraphiteClient` speaks the carbon plaintext protocol, where every metric is
one `<path> <value> <timestamp>` line and `;` separates tags. The perf-data
path and every metric value were already scrubbed so text from a check (a
check alias, a perfdata label) cannot embed a newline or `;` — but the
**status path** was only scrubbed for spaces. The `${check_alias}` substituted
into it can originate from a remote submitter (an NSCA passive result, a
forwarded submission), so an alias carrying a newline injected an extra,
attacker-chosen metric line into Graphite, and a `;` injected carbon tags.
Corrupting a metric another system alerts on, or planting a fake one, is a
way to hide a real problem or fabricate one.

The status path now goes through the same scrub as the perf path (`\n`, `\r`,
tab, `;`, NUL, spaces and the other reserved characters all become `_`), and
the scrub itself additionally replaces tabs, which split a carbon field the
same way a space does. No operator action is needed; a status path built from
an alias containing these characters (which previously produced corrupt lines)
now renders them as `_`.

### CheckExternalScripts security-review hardening

**Fixed in:** unreleased (first release after 0.18.0) · **Severity:** Low–Medium

A review of the `CheckExternalScripts` module and the process launcher it uses
produced a set of hardening fixes. None is a remotely exploitable RCE on
a default install — the argument guard is off by default, arguments allowed only
when an operator opts in, and the script-management operations require an
authenticated administrator — but each closes a rough edge.

- **`ext-scr install --arguments=...` now writes to the path the module reads.**
  The argument-lockdown helper read and wrote `allow arguments` /
  `allow nasty characters` under `/settings/external scripts/server`, a path
  nothing consults; the module reads them from `/settings/external scripts`. So
  `install --arguments=false` printed and "applied" a lockdown that had no
  effect — on a host with an existing `allow arguments = true` it left arguments
  enabled while reporting they were disabled. It now updates the effective path.
- **The command timeout is enforced on every path, and output is bounded.** On
  Unix the single-string shell-fallback ran through `popen()`, which hid the
  child PID: a hung script blocked forever with `timeout=` silently unenforced,
  wedging a worker thread per invocation. On Windows the read loop counted
  iterations rather than elapsed time, so a continuously-chatty script escaped
  the timeout entirely and leaked an unkillable process each run. Both launchers
  now bound the wait by wall-clock deadline and cap captured output at 8 MiB,
  removing a low-effort resource-exhaustion vector for anyone able to trigger a
  misbehaving check.
- **The show/delete sandbox resolves symlinks.** `ext-scr show` / `delete`
  bounded their target with a lexical path check that normalised `..` but did
  not resolve symlinks, so a symlink placed inside the script root pointing
  outside it passed — letting an authenticated admin read or remove files
  anywhere the service account could reach. The target and root are now
  canonicalised before the containment test.
- **`%` and `^` are blocked on the shell-fallback path.** The stricter
  metacharacter set applied to user arguments when a command degrades to the
  shell fallback omitted cmd.exe's `%VAR%` expansion and `^` escape character,
  so an argument reaching a `.bat` through cmd could smuggle environment
  contents or escape sequences into the command line. Both are now refused on
  that path (opt-out via `allow nasty characters`).
- **Documentation of two boundaries was corrected.** The `script root` setting
  claimed scripts cannot be uploaded/downloaded outside the folder, but `add`
  never enforced that; and `script path` turns every file in a directory into a
  runnable command. Both descriptions now state the real boundary, and
  `allow arguments` documents that it governs external scripts, not aliases.

**What to do:** if you rely on `ext-scr install` to lock arguments down, re-run
it after upgrading to write the effective setting (or set
`/settings/external scripts` `allow arguments` / `allow nasty characters`
directly). Treat write access to any `script path` directory, and the ability to
configure external-script commands, as equivalent to code execution as the
service account.

### NRDP client: transport hardening and shared TLS version-floor fix

**Fixed in:** unreleased (first release after 0.18.0) · **Severity:** Low–Medium

A follow-up security review of the `NRDPClient` module (after the 0.18.0
hardening below) produced three fixes on the submission transport. None is a
confirmed remote vulnerability in a default configuration; each closes an
availability or downgrade gap that a hostile, compromised or merely broken
NRDP endpoint could lean on. The TLS fix is in the shared TLS stack and
benefits every module that uses it.

- **A stalled NRDP server can no longer wedge passive monitoring.** The
  per-target `timeout` (and `retry`) settings were parsed but never applied to
  the HTTP exchange — every socket operation waited forever, so an NRDP server
  (or proxy) that accepted the connection and then stalled blocked the
  submission thread indefinitely, silently stopping passive results until a
  service restart. The configured timeout now bounds every step of the
  exchange — connect, TLS handshake, proxy `CONNECT` tunnel setup, request and
  response — and transport failures (and only transport failures) are retried
  up to `retry` times, each attempt on a fresh connection. As part of this,
  `timeout` and `retry` are read from the right place: both were also looked
  up in the free-form option map while the framework routes them into typed
  fields, so a configured value was ignored even where the timeout *was*
  honoured.
- **An NRDP response can no longer exhaust the agent's memory.** The response
  body was buffered without any size limit, so a hostile or compromised
  server — or a man-in-the-middle on a plain-`http://` target — could stream
  an arbitrarily large reply into the submitting process. The submission path
  now uses the size-capped fetch path (5 MB, far above any real NRDP reply)
  and reports an HTTP error status as such (`NRDP server at host:port returned
  403: Forbidden`) instead of feeding the error page to the XML parser.
- **`tls version = 1.2+` now actually means "TLS 1.2 or later".** The parser
  stripped the `+` and mapped the value onto a version-pinned OpenSSL method,
  which pins the *maximum* protocol version too — so the widely-used default
  `1.2+` negotiated TLS 1.2 only and silently excluded TLS 1.3. `+` values now
  resolve to the generic TLS method with an enforced minimum-version floor,
  and `any` (documented but previously rejected) is accepted. This fix covers
  the shared stack: all HTTP-based clients (NRDP, Elastic, Docker, web
  fetches), the NRPE/NSCA socket clients and servers, and `check_tcp`.

**What to do:** nothing required. A submission to an unresponsive server now
fails (and is retried) after `timeout` — 30 seconds for configured targets —
instead of hanging; raise `timeout` on a target if your NRDP endpoint is
legitimately slower than that. Endpoints negotiating with a `+` or `any` TLS
version can now select TLS 1.3; pin an exact version (`tls version = 1.2`)
in the unlikely case a peer misbehaves on 1.3.

### NSCA-NG client: cert-mode TLS settings (peer verification included) were never applied

**Fixed in:** unreleased (first release after 0.18.0) · **Affected:** 0.18.0, `NSCANgClient` in cert mode only · **Severity:** High for cert-mode targets; the default PSK mode is unaffected

A security review of the NSCA-NG client found that in certificate mode
(`use psk = false`) the TLS configuration was applied to the OpenSSL
context *after* the TLS stream had been created from it. `SSL_new()` copies
the verify mode, certificate, cipher list and protocol-version bounds out of
the `SSL_CTX` at creation time (only the CA store stays shared), so none of
that configuration reached the live connection:

- **`verify mode = peer-cert` had no effect** — the connection ran with
  verification off and accepted any certificate the server presented,
  including a man-in-the-middle's. The host-name check was installed but is
  advisory when the underlying verify mode is off, and the client's own
  "refusing to connect unverified" guard read the *configured* mode rather
  than the live one, so it passed too.
- **The configured client certificate and key were never presented**, so
  mutual-TLS setups authenticated in one direction only (and fail against
  servers that require a client certificate).
- The `tls version` floor and `allowed ciphers` list were likewise ignored
  in cert mode.

The context is now fully configured before the connection object is created
(the same ordering the other TLS client modules use), unit tests assert the
settings actually arrive on connections created from it, and integration
tests drive cert mode against a live TLS endpoint both ways: a wrong CA must
fail the handshake, and a mutual-TLS round-trip must succeed. The same fix
removes a rare undefined-behaviour window in the connection's I/O-timeout
handling (a stale deadline handler could fire into a later operation).

Not affected: the default PSK mode (`use psk = true`) configures the SSL
object directly and always authenticated both ends via the pre-shared key;
targets that never set `use psk = false` need no action.

**What to do:** upgrade if any NSCA-NG target uses `use psk = false`. After
the upgrade, a cert-mode target whose server certificate does not chain to
the configured `ca` (or does not match the host name) will fail to connect —
that is the verification taking effect; fix the server certificate or, if
you accept the MITM exposure, opt out explicitly with `insecure = true`.
### Elastic client: server certificate verification, authentication and modernization

**Fixed in:** unreleased (next release after 0.18.0) · **Severity:** Medium

A security review and modernization of the `ElasticClient` module (which
forwards events, metrics and the NSClient++ log to Elasticsearch):

- **HTTPS submissions now verify the server certificate.** The module
  hardcoded TLS verification to `none`, so an `https://` address validated
  neither the certificate chain nor the host name — an on-path attacker could
  read everything the agent ships (event-log entries, metrics and the agent
  log, which can carry sensitive operational detail) and forge responses.
  Verification now defaults to `peer` against the platform CA bundle
  (`${ca-path}`), with new `tls version`, `verify mode` and `ca` settings
  matching the other HTTP client modules.
- **The module can now authenticate.** Elasticsearch has shipped with
  security enabled by default since 8.0, but the module could not send
  credentials at all — forcing anonymous-write clusters. New `user`/`password`
  (basic authentication) and `api key` settings add the `Authorization`
  header; the values are stored as password-typed settings and never written
  to the trace log.
- **A stalled server can no longer wedge the agent.** Submissions ran with no
  network timeout, so an Elasticsearch server (or man-in-the-middle) that
  accepted the connection and went silent blocked the submitting thread
  forever. A new `timeout` setting (default 30 seconds) bounds every
  connect/read/write.
- **Documents in a batch no longer overwrite each other.** Every document in
  a bulk request was given the *same* random `_id`, so when one event message
  carried several entries only the last survived in Elasticsearch — silent
  data loss in the shipped audit trail. Each document now gets its own id.
- **Error responses are parsed defensively.** The response body (attacker-
  influenced on an unverified link, and previously parsed with throwing
  accessors) is now handled without assuming a shape, and error text included
  in the agent log is truncated.

**What to do:** nothing for plain `http://` addresses (the common loopback
setup). If you submit to an `https://` Elasticsearch endpoint with a
self-signed certificate, set `ca` to the certificate (or explicitly set
`verify mode = none` to keep the old insecure behaviour). If you run
Elasticsearch 6.x or older, set `event type`, `metrics type` and
`nsclient log type` explicitly — the legacy `_type` parameter is no longer
sent by default because Elasticsearch 8+ rejects it.

### NSCA client and server security-review hardening

**Fixed in:** unreleased · **Severity:** Low–Medium

A focused security review of the NSCA passive-check modules (`NSCAClient`,
`NSCAServer`) and the shared legacy crypto helper produced four hardening
changes. None is a remote-code-execution risk; the common theme is that a
misconfiguration could silently disable protections the operator thought were
enabled. (The protocol-level limitations of NSCA v3 — CRC32 instead of a MAC,
password-as-key with no key derivation — are inherent to the wire format;
prefer the `ssl` transport options or NSCA-NG where both ends support them.)

- **An unrecognized `encryption` value is no longer treated as "no
  encryption".** Any value the crypto helper did not recognize — a typo such
  as `aes-256`, or an algorithm not compiled into the build — historically
  resolved to *no encryption* on the end carrying it, with no warning. In
  practice a one-sided typo was unlikely to result in accepted plaintext:
  the ciphers have to match, so the correctly configured peer failed the
  CRC check and rejected every submission, and the misconfiguration
  surfaced as missing results. The exposure was narrower: a typo'd client
  still put each attempted submission on the wire in the clear before the
  mismatch was noticed, a typo'd server accepted unauthenticated cleartext
  packets from any allowed host for as long as the misconfiguration lasted,
  and only the same unrecognized value on *both* ends (one broken
  configuration copied to the other) ran plaintext indefinitely while
  looking encrypted. An unknown or unavailable algorithm is now a hard
  error that names the actual problem: the `NSCAServer` module refuses to
  load (with a log line listing the available algorithms) and an
  `NSCAClient` submission fails with the same message in the submission
  response, instead of an unexplained CRC mismatch on the other end.
  `encryption = none` (or an empty value) remains the explicit way to run
  unencrypted.
- **An empty password with encryption enabled now logs a loud error.** The
  NSCA key is the password zero-padded to the cipher's key length — there is
  no key-derivation step in the protocol — so `encryption = aes256` with the
  default empty `password` encrypts under a well-known all-zero key: anyone
  able to reach the port past `allowed hosts` can decrypt, forge and inject
  passive results for any host. Both the server (at startup) and the client
  (at submission) now log an error-level warning describing exactly that.
  Behaviour is otherwise unchanged; set the same `password` on both ends to
  clear the warning.
- **`performance data = false` on the NSCA server works again.** The setting
  was read and logged but never applied: perfdata was forwarded to the inbox
  channel regardless. It is now stripped from the submission when the setting
  is off, as documented.
- **Inbound wire fields are validated before they reach logs and the inbox
  channel.** The host name, service name and return code in an NSCA packet
  are attacker-influenced. ASCII control characters (including newlines and
  escape sequences, a log-injection vector) are now stripped from the host
  and service fields, and a return code outside the Nagios range 0–3 is
  clamped to 3 (UNKNOWN) instead of flowing downstream as an arbitrary
  16-bit integer.

**What to do:** nothing for a default install (aes256 with a shared password).
If either end carries a typo'd `encryption` value, or the build lacks
crypto++, encryption was previously silently off on that end (visible as the
peer rejecting submissions with a CRC error when it was configured
correctly) — fix the algorithm name, or set `encryption = none` if plaintext
was genuinely intended. If you see the new empty-password error in the log,
set the same `password` on both ends. If you counted on
`performance data = false` while it was broken, note that perfdata now
really is dropped.

### SMTP client security-review hardening

**Fixed in:** 0.18.0 · **Severity:** Low–Medium

A review of the `SMTPClient` module produced a set of fixes to how it sets up
and trusts a submission session. None is remotely exploitable by an
unauthenticated third party on its own — the attacker positions required are a
man in the middle on the path to the mail server, or the ability to submit a
passive result the agent relays to a mail target — but each removes a way the
agent can be made to trust something it should not.

- **Data pipelined across the STARTTLS handshake is refused.** The client kept
  one receive buffer for the whole session, and `read_until()` consumes whole
  segments, so bytes appended to the server's `220` STARTTLS greeting stayed
  buffered across the handshake and were then served to every read that
  followed. Those bytes arrive in cleartext and are writable by anyone who can
  inject a packet, yet the rest of the session treated them as though they had
  come from inside the TLS tunnel — the STARTTLS command/response injection
  family ([CVE-2011-0411](https://nvd.nist.gov/vuln/detail/CVE-2011-0411) and
  relatives). Beyond forged capabilities after the upgrade, a prepared run of
  `2xx` replies walks the client through `MAIL`, `RCPT` and `DATA` and has it
  report a notification as delivered when nothing was ever sent, which for a
  monitoring agent means alerts that silently go nowhere. Per RFC 3207 §4 the
  session is now refused if anything is buffered at handshake time.
- **The EHLO name is validated before it reaches the wire.** The envelope
  addresses and the subject were all checked for CRLF injection; the EHLO
  argument was interpolated into the command line raw. It defaults to the
  submitting sender's host name, which for a relayed submission comes from the
  request header, so a CR or LF in it ended the `EHLO` and began a command of
  the sender's choosing on a session the agent had already authenticated —
  their `MAIL FROM` and `RCPT TO`, sent as the agent. Only a host name or an
  address literal is accepted now, and the check runs before any socket opens.
- **Server certificates are verified against the agent's CA bundle.**
  Verification rested on OpenSSL's compiled-in default verify paths, which on
  **Windows do not include the Windows certificate store**. The default
  `security=starttls` therefore failed verification against Gmail, Microsoft
  365 and every other public-CA submission service on every Windows agent, and
  the module's only escape hatch was `insecure-skip-verify` — so the
  configuration operators arrived at was the one with verification turned off.
  A `ca` setting now defaults to `${ca-path}` like every other TLS client in
  the tree. See the [upgrade note](../setup/upgrading.md#0172).
- **ESMTP capabilities are matched per line rather than by substring.** Reply
  lines were concatenated with no separator and searched with `find()`, so a
  server's free text could answer for a capability it never advertised — a
  greeting naming the host `starttls.example.com` satisfied the STARTTLS
  lookup, and the seam between two joined lines formed tokens no line
  contained. That lookup is what decides whether the session gets encrypted
  before `AUTH`.
- **The timeout bounds the whole submission.** Every operation took a fresh
  deadline, and connect took one per resolved address, so a peer answering
  just inside the deadline on each round trip — or a name resolving to several
  black-holed addresses — held a submission thread for a large multiple of the
  configured timeout. Name resolution ran outside the deadline entirely.
- **`insecure-skip-verify` no longer resets itself.** Declared as a
  `bool_switch`, its notifier fired with `false` on every submission that did
  not name the option, overwriting whatever the target had configured, so the
  setting never worked from configuration at all. It also rejected the valued
  `insecure-skip-verify=true` token that REST passes. The reset failed
  *towards* verifying, so this was not a hole — it is why the setting appeared
  to do nothing.
- **`--source-host` no longer redirects the connection.** Shared by every
  client module, it was registered against the destination container, where
  the well-known `host` key routes into the typed address field — so naming a
  source host pointed the client at that host instead of the configured
  server, sending the submission (credentials included, for a module that
  authenticates) somewhere the operator did not intend. It binds to the sender
  now. `SMTPClient` and `NRDPClient` were not exposed: each registered its own
  competing copy, which made the option ambiguous and refused outright.

**What to do:** nothing is required on unix, where `${ca-path}` resolves to the
distribution's own CA bundle. On **Windows**, an SMTP target that was working
only because `insecure-skip-verify = true` should have that removed and
retried — certificate verification now succeeds against public providers. A
target pointing at an internal relay with a private CA should name that bundle
in `ca` rather than waive verification. See
[Upgrading](../setup/upgrading.md#0172) for the full list.
### NRPE: decoded-argument metachar guard, optional version banner, and consistency fixes

**Fixed in:** 0.18.0 · **Severity:** Low

A review of the `NRPEServer` / `NRPEClient` modules produced a set of
defense-in-depth fixes. None is remotely exploitable on a default install
(the listener still fails closed on `allowed hosts` and the argument guard
is off by default), but each tightens a rough edge:

- **Metachars are now re-checked on the *decoded* command and arguments.**
  The `allow nasty characters = false` guard previously ran only on the raw
  wire bytes, before the configured `encoding` was applied. With a non-UTF-8
  `encoding` set, a multi-byte sequence could decode into an ASCII shell
  metacharacter (`|`, `` ` ``, `&`, `>`, …) that was not literally present on
  the wire and so slipped past the ingress filter. The guard now also runs on
  the decoded strings that are actually dispatched. Downstream
  `CheckExternalScripts` argv isolation already prevented shell execution, so
  this closes a filter-bypass, not an RCE.
- **The unauthenticated `_NRPE_CHECK` ping reply can stop disclosing the
  build.** A new `expose version` server setting (default `true`, preserving
  the legacy banner `check_nrpe` expects) lets operators return a generic
  "doing fine" message instead of the exact NSClient++ version to any host
  permitted to open the port.
- **`nscp nrpe install` now reads the stored `verify mode`.** It previously
  queried the value but matched it under the wrong key name, so a re-run
  silently reset peer verification to its built-in default (`peer-cert` — a
  fail-secure direction, but it discarded the operator's setting).
- A cosmetic `std::wstring::npos` vs `std::string::npos` comparison in the
  metachar guard was corrected (both equal `SIZE_MAX`, so behaviour was
  unchanged).

**What to do:** nothing required. To harden further, set
`expose version = false` and prefer client-certificate authentication over
IP-only filtering — see
[Active Monitoring with NRPE](../scenarios/nrpe.md#securing-the-nrpe-endpoint).
The decoded-metachar re-check can reject a request that a non-UTF-8
`encoding` previously let through while `allow nasty characters = false`;
such a request was meant to be blocked, so this only affects inputs the
guard was always intended to catch.

### NRDP client security-review hardening

**Fixed in:** 0.18.0 · **Severity:** Low–Medium

A focused security review of the passive submission protocols (`NRDPClient`,
`NSCAClient`/`NSCAServer`) produced a small set of defense-in-depth fixes on
the NRDP side. None is a confirmed remote vulnerability in a default
configuration; they close latent gaps and bring NRDP in line with the
redaction already applied to NSCA.

- **A malformed NRDP server response can no longer crash the submitting
  agent.** `nrdp::data::parse_response` dereferenced the text node of the
  `<status>`/`<message>` elements without a null check, so a response
  containing an empty element (`<status></status>`) — which anyone able to
  answer or tamper with the connection can send, including a man-in-the-middle
  on a plaintext or unverified link — dereferenced a null pointer and crashed
  the process rather than throwing. It is now reported as an invalid response.
- **The NRDP token is no longer written to the trace log.** The per-target
  configuration is emitted at trace level on every submission and included the
  `token` — a shared secret equivalent to the NSCA password, which was already
  redacted. It now logs `<set>`/`<unset>`, and any credentials embedded in a
  configured `proxy` URL (`http://user:pass@host`) are redacted to
  `<redacted>@host`.
- **HTTPS submissions no longer silently skip certificate verification when no
  verify mode is set.** An unset `verify mode` resolves to `verify_none` in the
  TLS layer. Configured targets already defaulted to `peer`, but the bare
  `nscp client`/REST submission path did not, so an `https://` submission made
  that way validated neither the certificate chain nor the host name and
  exposed the token to a man-in-the-middle. That path now defaults to `peer`
  as well; plain `http://` is unaffected.

**What to do:** nothing required for the default install or for targets
configured through settings. If you submit to an `https://` NRDP endpoint via
`nscp client`/REST against a self-signed certificate *without* a configured
verify mode, add `--verify none` (or a CA) explicitly — the previous
behaviour was to trust any certificate silently.

### WEB server security-review hardening

**Fixed in:** 0.18.0 · **Severity:** Low–Medium

A focused security review of the `WEBServer` module (REST API + web UI)
produced a set of defense-in-depth fixes. None is a confirmed remote
vulnerability in a default configuration; they close latent gaps and
consistency issues.

- **Session tokens are drawn from the OpenSSL CSPRNG.** `token_store::generate_token`
  minted bearer session tokens (and generated admin passwords) with
  `std::random_device`, which the C++ standard does not require to be
  non-deterministic — some toolchains have historically implemented it as a
  fixed-seed PRNG, which would make tokens predictable. Tokens now come from
  `RAND_bytes()` (the same CSPRNG already used for password salts), with
  rejection sampling to keep the alphabet unbiased. A build *without* OpenSSL
  still uses `std::random_device` (it cannot serve TLS either); a build that
  has the CSPRNG never silently substitutes the weaker source — if
  `RAND_bytes()` fails, **no token or generated password is issued at all**
  and the request fails with a 500 (`nscp web add-user` / `nscp web install`
  report an RNG failure). Refusing is the correct outcome for a bearer
  credential, but it does mean a broken OpenSSL RNG blocks logins rather than
  degrading them; the failure is logged as a `SECURITY:` error.
- **Cookie name matching now requires a name boundary.** The bundled
  `mg_get_cookie` matched a requested cookie name as a substring, so a
  client cookie named `eviltoken` could satisfy a lookup for `token`. It now
  requires the match to start at a cookie boundary. The WEBServer module
  authenticates from headers, not request cookies, so this path was not used
  for authentication — the fix hardens the exported helper regardless.
- **The legacy `/auth/logout` route enforces the allowed-hosts perimeter.**
  Its sibling `/auth/token` already called `is_allowed()`; logout did not, so
  a host outside `allowed hosts` could revoke an observed token with a
  spoofed `User-Agent`. It now applies the same check.
- **Script / module names may no longer begin with `-`.** A leading dash could
  be mistaken for an option flag when the name is later passed as an argument
  value; interior dashes (`check-disk`) are unchanged.
- **The web-UI installer refuses an HTTPS→HTTP redirect.** The bundle's
  integrity rests on the TLS channel (the SHA-256 manifest is fetched over the
  same connection), so a redirect chain that began on `https://` is no longer
  allowed to drop to cleartext. A protocol-relative `Location` (`//host/path`)
  is now resolved as such, inheriting the current scheme, instead of being
  glued onto the current origin as a path — which both mis-resolved the
  redirect and hid it from the downgrade check. An operator who explicitly
  passes an `http://` `--url` is unaffected.
- **The `legacy` grant's startup warning now names `/settings/query.pb`.** In
  addition to the query/RCE routes it already flagged, the `SECURITY` warning
  now states that `legacy` also unlocks `POST /settings/query.pb`, which reads
  settings *without* the redaction the `/api/v2/settings` endpoints apply and
  can write settings — without holding `settings.get`/`settings.put`. This is
  a documentation fix; the endpoint's behaviour is unchanged, and `legacy` was
  already an RCE-equivalent, trusted-only grant.

**What to do:** nothing required for the default install. If you have a custom
script/module name that begins with `-`, rename it. If a client outside
`allowed hosts` relied on the legacy `/auth/logout` route, move it inside the
perimeter. Review any role that grants `legacy` — it confers unredacted
settings read and settings write in addition to command execution.

### Host name placeholders are sanitized before they land in a local path

**Fixed in:** 0.17.0 · **Severity:** Low

With host name placeholders now resolving in attachment target paths and in
`[/includes]` (issue [#458](https://github.com/mickem/nscp/issues/458)), the
system host name is substituted into paths the agent reads and writes with its
service privileges (typically root / SYSTEM). The host name is not fully under
the operator's control — DHCP can set it on some systems, as can any local
privileged process — so a hostile value such as `../../etc/cron.d/evil` must
not be able to redirect where an attachment is written or which file an
include opens. Values substituted into a path are therefore reduced to the
characters a legal RFC-952 host name can contain (letters, digits, `.`, `-`,
plus `_`): anything else becomes `_`, and a dots-only value (`.`/`..`) becomes
`_`. A legitimate host name comes through unchanged; settings urls and the
submit clients' host name specs are not affected, since there the value never
names a local file. The same expansion pass no longer aborts settings boot if
`gethostname()` itself fails — the placeholders are then left unresolved.

**What to do:** nothing required. Only a host name that is not a legal host
name resolves differently, and only in attachment targets and `[/includes]`.

### Settings values for sensitive keys are redacted on read

**Fixed in:** 0.16.2 · **Severity:** Medium · **Reported by:** [yagust](https://github.com/yagust)

The settings read paths — `GET /api/v2/settings/...`,
`GET /api/v2/settings/descriptions/...`, and the `nscp settings --list` /
`--show` CLI — returned values for keys registered sensitive (via
`add_password` / `is_sensitive_key`) in clear text, while the settings `diff`
endpoint already masked them. They now return `***` for sensitive keys, to
match `diff`; internal reads a module makes of its own configuration are
unaffected. This is a defense-in-depth / consistency change, not an
authorization boundary: the values remain stored in plaintext in
`nsclient.ini` (shared with the legacy NRPE/NSCA/NSClient protocols) and are
readable by any principal with write or execute privilege regardless.

**What to do:** nothing required. Tooling that read a secret back out of
`GET /api/v2/settings/...` now receives `***` for keys registered sensitive.

### The `legacy` WEB permission is flagged and no longer seeded by default

**Fixed in:** 0.16.2 · **Severity:** Medium · **Reported by:** [yagust](https://github.com/yagust)

The `legacy` WEB grant unlocks the deprecated `POST /query.pb` and
`GET /query/{name}` endpoints, which dispatch through the same command registry
as the versioned `/api/v2/queries` API — so a token holding the `legacy`
permission can run any registered check or command (including any configured
`CheckExternalScripts` command) even without `queries.execute`. This is
intended compatibility behaviour for clients that predate the versioned API,
but it was under-documented and more powerful than the role name suggests.

The built-in `legacy` role is no longer seeded on fresh installs; NSClient++
now logs a `SECURITY` warning at startup — and from `nscp web add-role` /
`add-user` — for any role whose grant includes the `legacy` token (so a custom
role such as `reporting = legacy,login.get` is flagged too); and the capability
is documented in the securing guide.

**What to do:** existing installs are unaffected (the role stays in their
config and keeps working). Only grant `legacy` to trusted legacy systems that
cannot use the versioned `/api/v2/queries` endpoints; see
[Securing NSClient++](../setup/securing.md).
