#### About `nrpe_query`

`nrpe_query` runs a command on a remote host over **NRPE** and returns its
result. It is the same command as [`check_nrpe`](#check_nrpe) under a second
name — the two are registered as aliases of one implementation, take the same
options and behave identically.

Both names exist because `check_nrpe` matches the classic Nagios plugin that
most people type from a shell, while `nrpe_query` follows this module's
`<protocol>_query` naming alongside `exec_nrpe`, `submit_nrpe` and
`nrpe_forward`. Pick whichever reads better in your configuration and stay
consistent.

See [`check_nrpe`](#check_nrpe) for the full description: targets, protocol
versions and payload length, TLS, and batching.
