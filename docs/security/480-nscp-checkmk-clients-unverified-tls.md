---
title: "NSCP and check_mk clients sent credentials over unverified TLS"
fixed_in: 0.23.0
severity: "Medium for NSCPClient and check_nsclient_web_online, Low for CheckMKClient"
modules: [NSCPClient, CheckMKClient, CheckNet]
action: conditional
---
Three outbound TLS paths negotiated an encrypted session and then accepted
whatever certificate the peer presented:

- **`NSCPClient`** — the agent-to-agent relay. TLS is on by default (the
  target is another agent's REST API on port 8443), but `verify mode`
  defaulted to `none` and no trust anchors were loaded. The target carries the
  remote agent's API password, so an on-path attacker answering for the
  target's address received that password and could return any check result it
  liked.
- **`check_nsclient_web_online`** (`CheckNet`) — the same exchange as a single
  check, with `verify` defaulting to `none`. It sends the remote agent's
  password in a `password` or `Authorization` header.
- **`CheckMKClient`** — read `verify mode` with no default at all. An empty
  verify mode parses to `verify_none`, so a target that turned TLS on
  encrypted the agent section without authenticating the agent it came from.
  No credential is sent on this path, and TLS is off by default, which is why
  it rates lower.

In each case the connection was encrypted, so the exposure required an
attacker on the path (or able to answer for the configured address) rather
than a passive observer — but nothing in the handshake distinguished that
attacker from the real agent.

All three now default to `verify mode = peer` with `ca` defaulting to the
agent's configured bundle (`${ca-path}`), the same default the NRDP, Icinga,
Graphite, Elastic and Op5 clients already carried. An explicit `verify mode =
none` is still honoured, so an operator who has decided to accept an
unauthenticated link keeps that choice — it just has to be made.

**What to do:** because an NSClient++ agent generates a self-signed
certificate on first start, a relay or REST check pointed at a default agent
now fails the handshake. Point `ca` at that certificate and set `verify mode =
peer-cert`, point `ca` at your own CA and keep `verify mode = peer`, or set
`verify mode = none` to restore the previous behaviour. See the
[upgrade note](../setup/upgrading.md).
