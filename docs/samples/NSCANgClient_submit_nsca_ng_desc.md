#### About `submit_nsca_ng`

`submit_nsca_ng` submits a passive check result to an **NSCA-ng** server.
NSCA-ng is the modern replacement for NSCA: it authenticates both ends over TLS
with a shared identity and password rather than obfuscating the payload with a
shared cipher, and it carries much larger output.

The usual way to use it is to route results rather than call it by hand: give a
scheduled check `target=nsca-ng`, or add the module's channel to the channels a
check reports on. A direct call is mainly useful for verifying credentials and
TLS.

##### Identity and password

`identity` and `password` must match a client entry in the server's
`nsca-ng.cfg`. The identity is what the server uses to decide which hosts and
services this client may submit results for, so it is an authorisation
boundary — not just a label.

`host check = true` submits the result as a **host** check rather than a service
check, which is how you report host state through the same channel.

##### TLS

The connection is TLS, configured with `certificate`, `certificate key`, `ca`,
`dh` and `allowed ciphers`.

`insecure = true` disables peer verification. It exists for bringing up a new
deployment before the CA is in place; leaving it on removes the guarantee that
you are talking to your own server, which is the main thing NSCA-ng gives you
over NSCA. Point `ca` at the server's CA instead.

##### Output length

`max output length` defaults to 65536 bytes — far more than NSCA's 512-byte
payload — so long check output survives intact. It still has to be no larger
than the server's own limit; a value above what the server accepts truncates on
its side.

##### Custom relay commands

Handlers defined under `[/settings/NSCA-NG/client/handlers]` are registered
automatically as additional commands, following the same `submit_<alias>`
naming, so a relay with several destinations does not need a module instance per
destination.
