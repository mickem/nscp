---
title: "A request can no longer shape the connection a configured client credential travels over"
fixed_in: next
severity: "Medium"
modules: [NRDPClient, IcingaClient, SMTPClient, NSCAClient, NSCANgClient, NSCPClient, NRPEClient]
action: conditional
---
The guard introduced in 0.19.0 and extended twice since (see
[Client credentials could be sent to a caller-chosen host](notices.md#client-credentials-could-be-sent-to-a-caller-chosen-host),
[Two ways past the client host-override guard](notices.md#two-ways-past-the-client-host-override-guard)
and
[NRDP: a request-supplied proxy sent the configured token through a caller-chosen host](notices.md#nrdp-a-request-supplied-proxy-sent-the-configured-token-through-a-caller-chosen-host))
named the keys it refused: first the destination, then a credential hidden in
the address, then the proxy. Each round closed the hole someone had found and
left every key nobody had looked at yet open.

The keys still open were the ones that decide how well the credential is
protected rather than where it goes. A request could set `verify=none` on a
credentialed https target, leaving the destination exactly as configured while
making the agent accept any certificate presented for that name; `ca=`,
`tls-version=`, `allowed-ciphers=`, `insecure=` and NSCA's `encryption=`
behaved the same way. Unlike the destination and proxy cases these need an
on-path or DNS position to exploit, so they are weaker, but they are the same
failure: a caller with `queries.execute` over REST, or an NRPE peer with
`allow arguments = true`, deciding the security of a connection that carries
the operator's secret.

The rule is now stated the other way round, so it does not have to be
rediscovered a fourth time. While a target's own credential is what travels
and `allow host override` is off, a request may set only what it is explicitly
allowed to: the payload it is submitting, `timeout`, `retry`, a password or
token it brought itself, and `target=`. Everything else is refused and the
refusal names the key. A module marks its own message-shaped options safe in
code, so an option added in future is guarded from the day it is added rather
than from the day somebody notices.

**What to do:** nothing, unless requests of yours set a connection option on a
credentialed target. `verify=`, `ca=`, `tls-version=`, `allowed-ciphers=`,
`insecure=`, `encryption=` and the destination and proxy keys are now refused
there, as `host=` already was. The remedies are unchanged: pass the credential
with the request, configure the other destination as its own target and select
it with `target=`, or set `allow host override = true` on the target. Naming a
value the target already configures is not a change and is still accepted.
