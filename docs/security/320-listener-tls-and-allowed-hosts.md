---
title: "Listeners: the insecure NRPE cipher string, a key `nrpe install` never wrote, and `*` in allowed hosts"
fixed_in: next
severity: "Low"
modules: [NRPEServer, NRPEClient, core]
action: conditional
---
Three findings from a review of the network listeners and their TLS
configuration. None is a bypass; two are cases where the configuration did not
mean what it said, and one is an availability footgun.

#### The insecure NRPE preset claimed a protection it could not provide

The legacy preset's cipher string carried `!ADH`, with a comment saying that
kept anonymous suites out. It never did. In an OpenSSL cipher string `ADH` names
the anonymous *finite-field* Diffie-Hellman suites only; the anonymous
elliptic-curve ones are `AECDH`, and `!aNULL` is the alias covering both. And
`!aNULL` here would have left no usable suite at all: the insecure mode loads no
server certificate, so the anonymous suites are the only ones that can complete
a handshake. The mode is anonymous by construction, and it already logs an error
saying the traffic can be intercepted.

The string now says what the mode is (`ALL:!MD5:@STRENGTH:@SECLEVEL=0`, matching
what `nscp nrpe install --insecure` writes) instead of implying a check that is
not there. The secure preset gains `!aNULL` alongside `!ADH`, where it is belt
and braces — the default security level refuses every anonymous suite anyway.

#### `nscp nrpe install` wrote a key the server never reads

The command reported that NRPE was enabled via SSL while writing `ssl = true`
under `[/settings/NRPE/server]`. The server registers `use ssl`, so the value
was read by nothing. `use ssl` defaults to true, which is why a fresh install
was unaffected; on a host where an operator had previously set `use ssl = false`,
re-running `nscp nrpe install --ca … --verify peer-cert` claimed encryption and
client-certificate authentication while the listener stayed in plaintext. The
command now writes `use ssl`.

#### `allowed hosts` advertised `*` ranges the parser could not handle

The setting's help text and the permissions documentation have described
`192.168.1.*` for years. The parser treated it as a numeric address and
`make_address` threw — outside the `try` that wraps only the DNS branch, so the
exception escaped the refresh. With `cache allowed hosts = true` (the default)
it escaped module loading too: the plugin manager dropped the module and the
listener never started. With caching off it threw on every accept. Both fail
closed, so this was an outage rather than a bypass, but from a documented
syntax.

`*` is implemented now: `192.168.1.*` is `192.168.1.0/24`, `192.168.*` is
`/16`, `10.*` is `/8` and a bare `*` is every address. A `*` in the middle of an
address, or combined with an explicit `/mask`, is reported as a configuration
error, and an unparseable numeric entry is now an error beside the others rather
than an exception that takes the listener with it.

**What to do:** nothing required. If you have been avoiding `*` in
`allowed hosts` because it stopped a listener, it works now. If a host had
`use ssl = false` and you ran `nscp nrpe install`, check the value: the install
command was reporting TLS it had not enabled.
