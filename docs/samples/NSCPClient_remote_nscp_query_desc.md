#### About `remote_nscp_query`

`remote_nscp_query` runs a check on a remote NSClient++ agent over the NSCP
protocol and returns its result. It is the same command as
[`check_remote_nscp`](#check_remote_nscp) under a second name — the two are
registered as aliases of one implementation, take the same options and behave
identically.

Both names exist because `check_remote_nscp` reads naturally where a monitoring
configuration lists check commands, while `remote_nscp_query` follows this
module's `remote_nscp_*` naming alongside `exec_remote_nscp`,
`submit_remote_nscp` and `remote_nscpforward`. Pick whichever reads better in
your configuration and stay consistent.

See [`check_remote_nscp`](#check_remote_nscp) for the full description: when to
prefer NSCP over NRPE, targets, and the password and TLS options.
