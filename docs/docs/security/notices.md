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
| [GHSA-rhrw-79v5-jc5x](https://github.com/mickem/nscp/security/advisories/GHSA-rhrw-79v5-jc5x) | CVE-2025-34079 | High (7.8) | 0.5.2.35 and earlier | Authenticated remote code execution via `ExternalScripts`. |
| [GHSA-jr25-22p3-gm6r](https://github.com/mickem/nscp/security/advisories/GHSA-jr25-22p3-gm6r) | CVE-2025-34078 | High (7.8) | 0.5.2.35 and earlier | Local privilege escalation from plaintext credentials in the configuration file. |

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
