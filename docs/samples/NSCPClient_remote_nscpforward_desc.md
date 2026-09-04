#### About `remote_nscpforward`

`remote_nscpforward` passes a request through to a remote NSClient++ agent over
the NSCP protocol **as-is**, without interpreting it.

This is the relay command. Where `check_remote_nscp` builds a request from
`command=` and `argument=` options, `remote_nscpforward` takes a request that
has already arrived at this agent and re-sends it to another one, returning
whatever comes back. That makes this host a proxy: a monitoring server that can
reach it can, through it, reach agents it cannot address directly — the usual
case being a DMZ or a management segment where only one host is exposed.

Register it as the fallback for a target and the arrangement becomes transparent
to the monitoring server, which believes it is talking to the final agent.
Because NSCP carries the request and the response as structured data, a
forwarded result arrives with its status, message and performance data intact —
the relay is lossless in a way an NRPE relay is not.

Two things follow from "as-is" that are worth being deliberate about. Because
the request is not inspected, **whatever the caller asks for is what the far end
is asked to run** — the relay adds no filtering of its own, so restrict what may
be forwarded, and to where, on this host rather than assuming the hop is a
control point. And because the relay terminates one connection and opens
another, the far end sees this host as the client: any password or
certificate-based authorisation there applies to the relay, not to the original
caller.

Connection, password and TLS options are the same as for
[`check_remote_nscp`](#check_remote_nscp).
