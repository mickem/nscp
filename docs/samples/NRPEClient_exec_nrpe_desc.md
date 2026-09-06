#### About `exec_nrpe`

`exec_nrpe` sends an **execute** request to a remote host over NRPE, rather than
a query.

The distinction matters. A *query* (`check_nrpe` / `nrpe_query`) asks the remote
agent to run a check and return a status, message and performance data — the
normal monitoring interaction. An *execute* request invokes the remote agent's
command-line interface and returns its textual output: the equivalent of running
`nscp <something>` on the far end, used for administrative operations rather
than for checks.

**Most of the time you want [`nrpe_query`](#nrpe_query) instead.** The module's
own description says so, and reaching for `exec_nrpe` to run a check will give
you raw text with no status to alert on.

The options are the same as for `check_nrpe`: `host=` / `port=` / `address=` or
`target=` for the connection, `command=` and `argument=` for what to run, plus
the shared TLS options.

The remote agent must be willing to serve execute requests at all — an
NSClient++ agent exposes them only where its configuration allows, and a stock
Nagios `nrpe` daemon has no such concept. Since an execute request is closer to
remote administration than to monitoring, be deliberate about which hosts accept
it and from where.
