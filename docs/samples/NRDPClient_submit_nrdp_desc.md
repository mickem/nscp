#### About `submit_nrdp`

`submit_nrdp` submits a passive check result to a **Nagios Remote Data
Processor** endpoint — the HTTP(S) submission API used by Nagios XI and by NRDP
installations in front of Nagios Core.

The usual way to use it is to route results rather than call it by hand: give a
scheduled check `target=nrdp`, or add the module's channel to the channels a
check reports on. A direct call is mainly useful for verifying that the token,
URL and TLS settings are right.

##### The token

NRDP authenticates with a **shared token**, configured as `token` (`key` and
`password` are accepted as synonyms). It is the only credential, it is sent with
every submission, and it is per-endpoint rather than per-host — so treat it the
way you would a password: keep it out of command lines that end up in process
listings or logs, and put it in the module's settings instead.

##### Transport

Submissions go over HTTP or HTTPS depending on the configured target. Since the
token travels with every request, **use HTTPS**: `verify mode` defaults to
`peer` and `tls version` to 1.3, with `ca` pointing at the bundle used to verify
the server. If the NRDP endpoint sits behind a corporate proxy, `proxy` sets the
proxy URL and `no proxy` the list of hosts to reach directly.

Results are batched into one submission where several arrive together, so a
scheduler reporting many checks at once produces one HTTP round trip rather than
one per check.
