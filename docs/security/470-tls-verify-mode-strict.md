---
title: "Listener verify mode: unknown flags are rejected instead of dropped"
fixed_in: 0.23.0
severity: "High for listeners configured for mutual TLS, none otherwise"
modules: [NRPEServer, NSCAServer, CheckMKServer, NSClientServer, WEBServer]
action: required
---
The `verify mode` parser used by every listener accepted five spellings and
silently ignored everything else. `fail-if-no-peer-cert` was not one of the
five — but it is the spelling the permissions guide, the `client identity
source` help text and OpenSSL itself use, and it is what the NRPE scenario
walkthrough and the reference docs told operators to write.

So `verify mode = peer,fail-if-no-peer-cert` resolved to bare `verify_peer`.
The listener asked the client for a certificate and completed the handshake
when none arrived. An operator who had configured mutual TLS, and whose
configuration looked exactly like the documentation, was running an NRPE
listener authenticated by the `allowed hosts` IP list alone. Any typo in the
setting degraded the same way, always in the direction of accepting more.

Two changes:

* The listener parser now accepts the same vocabulary as the outbound client
  parser — `peer` or `certificate`, `fail-if-no-cert` or `fail-if-no-peer-cert`
  or `client-certificate`, plus `peer-cert`, `client-once`, `none`, and the two
  context options `workarounds` and `single` — and whitespace around a token is
  ignored.
* Any other token is rejected. The listener logs the offending token and does
  not start, rather than starting with whatever bits survived the typo.

`client identity source = cn` on `NRPEServer` already refused to start without
both `peer` and `fail-if-no-peer-cert` in the parsed mask, so a CN-based policy
was never driven by an unverified certificate. Listeners not using that mode
had no such guard.

**What to do:** check the `verify mode` of every listener you have configured
for mutual TLS before upgrading. If it contained a token this release does not
recognise, the listener will refuse to start and name the token in the log —
which is the point, but it is a restart away. If you believed a listener was
requiring client certificates, verify it now: a client with no certificate
should be rejected.
