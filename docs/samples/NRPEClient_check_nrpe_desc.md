#### About `check_nrpe`

`check_nrpe` runs a command on a remote host over **NRPE** and returns its
result — the active-check direction, and the command this module exists for.

`check_nrpe` and [`nrpe_query`](#nrpe_query) are the same command under two
names; use whichever reads better in your configuration.

Name the remote host with `host=` (and `port=`, or `address=host:port`), or with
`target=` to pull the connection details from a target defined in the module's
settings — which is where credentials and TLS material belong, rather than on
every command line. `command=` is the command the remote agent should run, and
`argument=` passes arguments to it (repeatable).

The remote end decides what `command=` means: an NSClient++ agent maps it to its
own check commands, a Nagios `nrpe` daemon to a `command[...]` line in
`nrpe.cfg`. A command the far end does not know is an error there, not here.

##### Protocol version and payload length

`version=` selects the NRPE protocol version and must match what the remote
daemon speaks. Version 2 is the classic protocol with a **fixed 1024-byte
payload**, which silently truncates longer check output; version 3 and later
negotiate a larger payload. `payload length=` sets the version-2 buffer and must
equal the remote daemon's compiled-in value — a mismatch corrupts the exchange
rather than reporting a clean error.

If a check works but its output is cut off at a suspiciously round length, this
is why.

##### TLS

NRPE's transport is TLS, configured with `certificate=`, `certificate-key=`,
`ca=`, `dh=`, `verify=` and `allowed-ciphers=`.

Classic NRPE deployments use **anonymous Diffie-Hellman** ciphers: encrypted, but
with no authentication of either end, so anyone who can reach the port can run
the daemon's commands and anyone on the path can impersonate the server. That is
what `insecure` mode preserves for compatibility with old daemons. Where both
ends are NSClient++, use real certificates and `verify=peer` instead.

##### Batching

`batch=` runs several commands in one connection, given as
`command|argument|argument` records separated by `separator=` (default `|`).
