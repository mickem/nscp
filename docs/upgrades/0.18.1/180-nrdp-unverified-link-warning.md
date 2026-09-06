---
icon: "🔒"
modules: [NRDPClient]
action: none
---
**An NRDP submission over an unverified `https` link now says so in the log.**
When a target's `verify mode` resolves to no peer verification, the module logs
a message naming the endpoint — the token then goes to whichever server answers
— matching what the Icinga client already does. The connection itself is
unchanged. The `verify mode` help text was corrected at the same time: it
listed three values the client-side parser rejects (`client-once`,
`workarounds`, `single`) and recommended `none` for self-signed certificates;
use `peer-cert` with `ca` pointing at the certificate instead. See the
[security notice](../security/notices.md#nrdp-submissions-warn-about-an-unverified-tls-link).
