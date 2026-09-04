---
title: "NRPE: decoded-argument metachar guard, optional version banner, and consistency fixes"
fixed_in: 0.18.0
severity: "Low"
modules: [NRPEServer, NRPEClient]
---
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
