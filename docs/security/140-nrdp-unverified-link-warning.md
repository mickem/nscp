---
title: "NRDP submissions warn about an unverified TLS link"
fixed_in: 0.18.1
severity: "Low"
modules: [NRDPClient]
action: none
---
An `https` NRDP submission whose `verify mode` resolves to no peer
verification now logs a message naming the endpoint, the same way an Icinga
submission does: the NRDP token is a shared secret, and an unverified TLS
session hands it to whichever server answers. Nothing about the connection
changes — an operator who set `verify mode = none` deliberately keeps getting
what they asked for, once per submission with a log line saying so.

The `verify mode` help text is corrected in the same pass. It advised
"peer-cert **or none** for self signed certificates" and listed `client-once`,
`workarounds` and `single` as accepted values; those three are rejected by the
client-side parser (the connection fails), and `none` disables verification
entirely rather than accommodating a self-signed certificate. The right answer
for a self-signed certificate is `verify mode = peer-cert` with `ca` pointing
at that certificate.

**What to do:** nothing required. If the new message appears for a target you
expected to be verified, that target is sending its token to an unauthenticated
peer — set `verify mode = peer`, or `peer-cert` with a `ca`.
