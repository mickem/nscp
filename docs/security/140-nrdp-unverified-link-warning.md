---
title: "NRDP submissions warn about an unverified TLS link"
fixed_in: 0.18.1
severity: "Low"
modules: [NRDPClient]
action: none
---
An `https` NRDP submission whose `verify mode` resolves to no peer
verification now logs a message naming the endpoint, as an Icinga submission
already does: the token is a shared secret, and an unverified TLS session
hands it to whichever server answers. The connection itself is unchanged.

The `verify mode` help text is corrected in the same pass. It recommended
"peer-cert **or none** for self signed certificates" and listed `client-once`,
`workarounds` and `single`, which the client-side parser rejects. For a
self-signed certificate use `peer-cert` with `ca` pointing at it; `none`
disables verification entirely.

**What to do:** nothing required. If the message names a target you expected
to be verified, set `verify mode = peer` (or `peer-cert` with a `ca`).
