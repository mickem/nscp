---
icon: "🔒"
modules: [NRPEServer]
---
**NRPE hardening: a new `expose version` setting, and the metachar
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
