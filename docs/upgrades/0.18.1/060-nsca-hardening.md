---
icon: "🔒"
modules: [NSCAServer, NSCAClient]
---
**NSCA hardening: empty-password warning, `performance data = false`
honoured, wire-field validation.** Enabling NSCA encryption with an empty
`password` now logs an error on both ends (the password *is* the key, so an
empty one is a well-known key) — set the same password on both ends to
clear it. `NSCAServer`'s `performance data = false` now actually strips
perfdata from forwarded submissions (it was silently ignored). Inbound
host/service names are stripped of control characters and out-of-range
status codes are clamped to UNKNOWN. No action needed on a default install.
See the [security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
