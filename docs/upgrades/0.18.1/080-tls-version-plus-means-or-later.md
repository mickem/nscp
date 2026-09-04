---
icon: "🔒"
modules: [core]
---
**A `tls version` with a trailing `+` now means "that version or later".**
`1.2+` (the common default) previously negotiated TLS 1.2 *only*; it now
also permits TLS 1.3, and `any` is accepted as the documentation always
claimed. This applies everywhere the setting exists: NRDP and the other
HTTP-based clients, the NRPE/NSCA clients and servers, and `check_tcp`.
No action needed; pin an exact version (`tls version = 1.2`) if a peer
misbehaves when TLS 1.3 is offered. See the
[security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
