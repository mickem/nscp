#### About `nrpe_forward`

`nrpe_forward` passes a request through to a remote host over NRPE **as-is**,
without interpreting it.

This is the relay command. Where `check_nrpe` builds an NRPE request from
`command=` and `argument=` options, `nrpe_forward` takes a request that has
already arrived at this agent and re-sends it to another one, returning whatever
comes back. That makes this host a proxy: a monitoring server that can reach it
can, through it, reach agents it cannot address directly — the usual case being
a DMZ or a management segment where only one host is exposed.

Register it as the fallback for a target and the arrangement becomes transparent
to the monitoring server, which believes it is talking to the final agent.

Two things follow from "as-is" that are worth being deliberate about. Because
the request is not inspected, **whatever the caller asks for is what the far end
is asked to run** — the relay adds no filtering of its own, so restrict what may
be forwarded, and to where, on this host rather than assuming the hop is a
control point. And because the relay terminates one TLS connection and opens
another, the far end sees this host as the client: any certificate-based
authorisation on the far end applies to the relay, not to the original caller.

Connection and TLS options are the same as for [`check_nrpe`](#check_nrpe).
