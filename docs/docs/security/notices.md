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
| Upstream ([Cesanta Mongoose](https://github.com/cesanta/mongoose)) | [CVE-2026-73256](https://nvd.nist.gov/vuln/detail/CVE-2026-73256), [CVE-2026-73257](https://nvd.nist.gov/vuln/detail/CVE-2026-73257) | Critical (9.1) | Windows builds up to 0.16.3 (bundled Mongoose < 7.22) | HTTP request smuggling in the bundled Mongoose web server; exploitable behind a reverse proxy / WAF. |
| [GHSA-rhrw-79v5-jc5x](https://github.com/mickem/nscp/security/advisories/GHSA-rhrw-79v5-jc5x) | CVE-2025-34079 | High (7.8) | 0.5.2.35 and earlier | Authenticated remote code execution via `ExternalScripts`. |
| [GHSA-jr25-22p3-gm6r](https://github.com/mickem/nscp/security/advisories/GHSA-jr25-22p3-gm6r) | CVE-2025-34078 | High (7.8) | 0.5.2.35 and earlier | Local privilege escalation from plaintext credentials in the configuration file. |

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
