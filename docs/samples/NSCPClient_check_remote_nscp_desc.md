#### About `check_remote_nscp`

`check_remote_nscp` runs a check on a remote NSClient++ agent over that agent's
**REST API** and returns its result.

`check_remote_nscp` and [`remote_nscp_query`](#remote_nscp_query) are the same
command under two names; use whichever reads better in your configuration.

##### What the remote end needs

The far end answers on its web server, so the remote agent needs the
`WEBServer` module enabled and reachable on the port you point at (`8443` by
default). The request goes to `/api/v2/queries/<command>/commands/execute` with
`Accept: text/plain`, and the answer is a Nagios result: the status in the HTTP
status code, `message|perfdata` in the body, one line per response line.

This replaces the older raw-protobuf `/query.pb` transport, which let the caller
write the request header and so pick the identity the remote's permission layer
attributed the call to. The versioned API takes the command and its arguments
and stamps the identity from the authenticated session instead.

##### Why use this instead of NRPE

- **No payload ceiling.** NRPE version 2 truncates output at a fixed buffer.
  This transport does not, and the remote builds its performance data without
  truncation, so a long check result arrives whole.
- **Real authentication.** A password over TLS against a named user with an
  explicit permission grant, rather than NRPE's traditional anonymous
  Diffie-Hellman. TLS is on by default here.

Use [NRPE](NRPEClient.md) when the far end is a Nagios `nrpe` daemon or another
non-NSClient++ agent; use this when it is NSClient++.

##### Connecting

Name the host with `host=` (and `port=`, or `address=host:port`), or with
`target=` to pull the connection details from a target defined in the module's
settings — which is where the password and TLS material belong, rather than on
every command line. `command=` names the check to run on the far end and
`argument=` passes arguments to it (repeatable), exactly as if you were running
that check locally.

A target may also set `path` to override the API base, which defaults to
`/api/v2/queries`. It is a target setting rather than a command-line option.

##### Security

`password=` is sent as the `password` header — the same one Icinga's
`check_nscp_api` uses. The remote maps it to its implicit `admin` web user, so
that user must exist with this password and hold a role granting
`queries.execute`. A fresh install seeds both; a hardened one may not, and the
check then comes back UNKNOWN rather than passing.

TLS is enabled by default and is configured with `certificate=`,
`certificate-key=`, `ca=`, `dh=`, `verify=` and `allowed-ciphers=`. Certificate
verification defaults to `none`, because an agent generates a self-signed
certificate on first start and requiring a verified peer would make every
default deployment fail — so out of the box the connection is encrypted but the
server is not authenticated. Set `verify=peer` and point `ca=` at the issuing
certificate to fix that. `ssl=false` turns TLS off entirely, which sends the
password in the clear.
