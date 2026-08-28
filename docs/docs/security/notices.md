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

**Fixed in:** 0.17.1 (bundles OpenSSL 3.5.8) · **Severity:** Moderate (highest fix reachable through NSClient++)

The **Windows builds** of NSClient++ statically bundle
[OpenSSL](https://github.com/openssl/openssl) as the TLS engine for the
NRPE/NSCA/NSClient server and client modules and the web server / REST API,
for certificate parsing in `check_certificate`, and for password hashing.
Releases up to 0.17.0 bundled OpenSSL 3.5.4; the four upstream security
releases since then — 3.5.5 (27 Jan 2026), 3.5.6 (7 Apr 2026), 3.5.7
(9 Jun 2026) and 3.5.8 (25 Aug 2026) — fix some forty CVEs on the 3.5 LTS
line, and 0.17.1 moves the bundled copy to 3.5.8.

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

**What to do:** upgrade Windows installs to 0.17.1 or later — promptly if
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

### NRDP client security-review hardening

**Fixed in:** 0.17.1 · **Severity:** Low–Medium

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

**Fixed in:** 0.17.0 · **Severity:** Low–Medium

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
