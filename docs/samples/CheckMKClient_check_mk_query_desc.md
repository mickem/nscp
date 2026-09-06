#### About `check_mk_query`

`check_mk_query` connects to a remote **check_mk agent**, retrieves its output
and hands it to a Lua script that turns it into a check result. It is the active
half of the check_mk support: the agent on the far end is a check_mk agent, not
NSClient++.

##### The Lua script does the work

Unlike the other client modules, `check_mk_query` does not itself interpret what
it fetched. The check_mk agent returns a sectioned plain-text dump — `<<<mem>>>`,
`<<<df>>>`, `<<<ps>>>` and so on — and a Lua script registered under the
module's `scripts` section is called back with the parsed packet to decide what
the status and message should be. `default_check_mk.lua` is loaded when no
script is configured.

That means the useful configuration for this command is mostly *not* on the
command line: what the check reports is whatever your script returns. Point
`scripts` at your own file when you want anything other than the default
behaviour.

##### Connection

The usual client options apply — `host=` (with `port=`, defaulting to the
module's configured value), or `target=` to use a target defined in the module's
settings, plus `timeout=`, `retries=` and the TLS options (`certificate=`,
`ca=`, `verify=`, `allowed-ciphers=`).

Note that a stock check_mk agent listens on **TCP 6556 in plain text**, with
access control done by source-IP allowlist rather than by authentication, so set
`port=6556` explicitly unless you have configured otherwise, and treat the
transport as unauthenticated unless you have put TLS in front of it.

See also the CheckMKServer module for the passive direction — serving check_mk
agent output *from* this host to a check_mk server.
