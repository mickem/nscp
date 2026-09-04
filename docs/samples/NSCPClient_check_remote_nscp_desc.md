#### About `check_remote_nscp`

`check_remote_nscp` runs a check on a remote NSClient++ agent over the **NSCP
protocol** and returns its result.

`check_remote_nscp` and [`remote_nscp_query`](#remote_nscp_query) are the same
command under two names; use whichever reads better in your configuration.

##### Why use this instead of NRPE

The NSCP protocol is NSClient++'s own agent-to-agent transport, and it is the
better choice when both ends run NSClient++:

- **No payload ceiling.** NRPE version 2 truncates output at a fixed buffer;
  NSCP carries the full result.
- **Structured results.** Status, message *and* performance data travel as
  protobuf rather than being squeezed into one Nagios line and re-parsed, so
  perf data survives intact.
- **Real authentication.** `password=` plus certificate verification, rather
  than NRPE's traditional anonymous Diffie-Hellman.

Use [NRPE](NRPEClient.md) when the far end is a Nagios `nrpe` daemon or another
non-NSClient++ agent; use this when it is NSClient++.

##### Connecting

Name the host with `host=` (and `port=`, or `address=host:port`), or with
`target=` to pull the connection details from a target defined in the module's
settings — which is where the password and TLS material belong, rather than on
every command line. `command=` names the check to run on the far end and
`argument=` passes arguments to it (repeatable), exactly as if you were running
that check locally.

##### Security

`password=` is the shared secret the remote agent requires. TLS is configured
with `certificate=`, `certificate-key=`, `ca=`, `dh=`, `verify=` and
`allowed-ciphers=`; set `verify=peer` with a real CA so the client actually
authenticates the server rather than merely encrypting to whoever answers.
