#### About `submit_nrpe`

`submit_nrpe` sends a **passive result** to a remote host over NRPE: instead of
asking the far end to run a check, it hands it a result that has already been
produced here.

**Most of the time you want [`nrpe_query`](#nrpe_query) instead** — the module's
own description says so. NRPE is fundamentally an active-check protocol, and
passive results normally travel over a transport designed for them, such as
[NSCA-ng](NSCANgClient.md), [NRDP](NRDPClient.md) or
[NSCA](NSCAClient.md). Use this only where the receiving end is an NSClient++
agent that accepts submissions over NRPE.

The result is described with `command=` (or its synonym `alias=`, the service
name to report against), `result=` (a number, or `OK` / `WARN` / `CRIT` /
`UNKNOWN`) and `message=`. `batch=` submits several results in one connection as
`command|result|message` records separated by `separator=` (default `|`).

Connection and TLS options are the same as for [`check_nrpe`](#check_nrpe), and
the same payload-length caveat applies: on protocol version 2 a message longer
than the negotiated buffer is truncated, so long check output submitted this way
may not arrive whole.
