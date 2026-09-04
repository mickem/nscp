#### About `exec_remote_nscp`

`exec_remote_nscp` sends an **execute** request to a remote NSClient++ agent over
the NSCP protocol, rather than a query.

The distinction matters. A *query*
([`remote_nscp_query`](#remote_nscp_query)) asks the remote agent to run a check
and return a status, message and performance data — the normal monitoring
interaction. An *execute* request invokes the remote agent's command-line
interface and returns its textual output: the equivalent of running `nscp
<something>` on that host, used for administrative operations rather than for
checks.

Reach for it to drive an agent remotely — inspecting its settings, listing its
modules, running a maintenance command — not to collect check results. Using it
for a check gives you raw text with no status to alert on.

The options are the same as for
[`check_remote_nscp`](#check_remote_nscp): `host=` / `port=` / `address=` or
`target=` for the connection, `command=` and `argument=` for what to run, plus
`password=` and the TLS options.

Because an execute request is closer to remote administration than to
monitoring, be deliberate about which agents accept it and from where; the
remote agent's own configuration decides whether it serves execute requests at
all.
