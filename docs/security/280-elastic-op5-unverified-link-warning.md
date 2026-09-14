---
title: "Elastic and Op5 submissions warn about an unverified TLS link"
fixed_in: next
severity: "Low"
modules: [ElasticClient, Op5Client]
action: none
---
`ElasticClient` and `Op5Client` both send credentials to an `https` endpoint —
Elasticsearch basic auth or an API key, an Op5 user and password — and both
honour a `verify mode` that can turn peer verification off. Until now they did
so silently: a link whose `verify mode` resolves to no peer verification hands
those credentials to whichever server answers the connection, with nothing in
the log to say so. The Icinga, NRDP and NRPE clients already warn in that
situation; these two now do too, naming the endpoint and the mode.

The warning is logged once per module load (not per submission) and only when
credentials are actually configured — without them the exposure is the
submitted data alone, which the `verify mode` setting description already
spells out. Nothing about the connection changes: `verify mode = none` stays
what the operator asked for.

The Icinga client's equivalent warning was logged on **every** submission,
which on a 60 s schedule buried it under 1440 copies a day. It is now gated
the same way the NRDP client's is: once per target and mode for as long as the
service runs.

The shared `--verify`, `--ca`, `--certificate-key` and `--allowed-ciphers`
option descriptions — used by `NRPEClient`, `NSCPClient`, `NSCAClient`,
`CheckMKClient` and `NSCANgClient` — read "Client certificate format" for
three of the four, so `--verify` documented nothing at all. They now describe
what they do, and `--verify` lists the accepted values and says what `none`
costs.

**What to do:** nothing required. If the message names an endpoint you
expected to be verified, set `verify mode = peer` (or `peer-cert` with `ca`
pointing at the certificate).
