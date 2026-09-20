---
title: "check_tcp completed TLS handshakes without verifying the server certificate"
fixed_in: 0.22.0
severity: "Low"
modules: [CheckNet]
action: conditional
---
`check_tcp` (and `check_ssh`, which shares its implementation) defaulted to
`verify = none` and loaded no trust anchors at all unless `ca=` was given. A
TLS check therefore completed its handshake against any certificate the peer
presented — expired, self-signed, issued for a different host, or issued by
anyone — and reported `ok`.

This is a monitoring check rather than a channel carrying credentials, so the
exposure is limited to what the check *reports*: an operator watching a TLS
service saw a green result from a service that a client would have refused to
talk to, and an interposed endpoint presenting its own certificate was
indistinguishable from the real one. `check_http` was not affected; it has
always defaulted to `verify = peer` against the agent's bundle.

Two further consequences of the same gap, now fixed:

- With no trust anchors loaded, `cert_verify` reported `unable to get local
  issuer certificate` for **every** publicly issued certificate, so a filter
  written as `crit=cert_verify != 'ok'` fired on precisely the well-configured
  servers it was meant to bless.
- `sni=` was accepted and ignored outside a TLS session, and `sans=` on a plain
  connection reported `ok`. Both returned a passing result for a check that had
  asserted nothing about any certificate.

`verify` now defaults to `peer`, `ca=` defaults to the agent's configured
bundle (`${ca-path}`) and falls back to OpenSSL's own trust store, `sni=`
without TLS is rejected, and `sans=` with no certificate reports
`san_missing`.

**What to do:** nothing, if your TLS checks target publicly issued
certificates — they validate out of the box now. If a check targets an
internal or self-signed certificate, point `ca=` at the issuing CA, or add
`verify=none` to keep reading the certificate without a trust decision. See
the [upgrade note](../setup/upgrading.md).
