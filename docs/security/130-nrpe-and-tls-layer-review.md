---
title: "NRPE and the shared TLS layer: key permissions, handshake deadline and codec fixes"
fixed_in: 0.18.1
severity: "High for the generated-key exposure, Low–Medium for the rest"
modules: [NRPEServer, NRPEClient, NSCAServer, NSClientServer, CheckMKServer, WEBServer, core]
action: conditional
---
An end-to-end review of the NRPE path — the wire codec, the server parser and
protocol, both modules, and the shared socket/TLS layer they sit on —
produced the fixes below. None is remotely exploitable for code execution
past `allowed hosts`; the key-permissions issue is the one that needs
operator action.

#### Generated TLS private keys were world-readable

`socket_helpers::write_certs` created the file with a plain
`fopen(cert, "wb")`, so it landed at `0666 & ~umask` — `0644` under a normal
systemd unit — and what it contained was an *unencrypted* PKCS#8 private key
followed by the certificate. Packaging never narrowed it either.

This was not a manual-only path: a default NRPE server start runs
`validate_certificate`, which **generates** the file when it is missing. So a
default install created a world-readable TLS key, and any local account could
read it, decrypt captured NRPE traffic or impersonate the agent.

The CA branch was worse. `write_certs(ca_path, true)` wrote the CA *private
key* into `ca.pem` — the file `nscp nrpe install` tells the operator to
distribute ("the clients need to have a certificate issued from …"). Anyone
who received that file could mint client certificates and walk straight
through `verify mode = peer-cert`, which is NRPE's only real authentication.

Generated key files are now created `0600` on Unix (and re-narrowed when they
already existed) and with a DACL restricted to `SYSTEM` and the local
`Administrators` group on Windows, breaking inheritance so a permissive
parent directory cannot widen them. A generated CA now writes only the
certificate to `ca.pem` and its private key to `ca-key.pem` beside it.

**What to do:** existing files are not modified by the upgrade. Check the
permissions of any certificate NSClient++ generated for you
(`chmod 600 …/security/certificate.pem`). If you distributed a generated
`ca.pem`, treat that CA as compromised — regenerate it and re-issue client
certificates.

#### The inbound TLS handshake had no deadline

`ssl_connection::start()` overrides the base `start()` — which is where the
connection timer is armed — and runs the handshake before it, so the entire
handshake phase was unbounded. A host permitted by `allowed hosts` could open
N sockets, send nothing, and pin N connection objects, file descriptors and
buffers forever. The plain-TCP path was bounded by `timeout` (30 s by
default); the SSL path, which is the NRPE default, was not. This is the same
class as the client-side "operations that could never time out" set fixed in
0.18.0, on the inbound side.

The deadline is now armed before the handshake and re-armed for the request
itself once it completes. The number of established connections is still only
bounded by the accept backlog, so `allowed hosts` remains the control that
matters for an untrusted network.

**What to do:** nothing required. A peer that cannot complete a TLS handshake
within the listener's `timeout` is now dropped; raise `timeout` if your
network needs longer.

#### The NRPE client never authenticated the server

`verify mode` defaults to `none` and `ca` is empty for every NRPE target, so
`ssl = true` buys encryption with no peer authentication whatsoever: whoever
answers the TCP connect is trusted, and on-path impersonation is
undetectable. (`insecure = true` goes further and drops `!ADH` from the
client cipher list, making the session explicitly anonymous.)

The asymmetry was worth calling out: the NRPE *server* argues at length for
keeping `!ADH` because anonymous Diffie-Hellman "gives encryption with no
peer authentication … can be MITMed undetected" — and the client default
produced exactly that outcome by another route. It was also not fixable out
of the box: a generated certificate carried
`SAN = DNS:localhost,IP:127.0.0.1`, while the client installs host-name
verification whenever `verify_peer` is set, so it could only ever verify
against localhost.

The default is unchanged — it is a compatibility decision, since most NRPE
servers in the field have no CA to verify against — but the client now logs
one error line per target and mode, at the first check against it, naming the
endpoint and what to set. Generated certificates now carry the machine's own
host name and the address a remote peer would see it as, alongside the
loopback entries.

**What to do:** if you want the server authenticated, set
`verify mode = peer-cert` and point `ca` at the issuer of the server's
certificate. A certificate generated before this release still has the
localhost-only SAN; delete it and restart to regenerate one that can be
verified.

#### 512-bit DH parameters are no longer shipped

`security/nrpe_dh_512.pem` was installed beside `nrpe_dh_2048.pem` and was the
default of `socket_helpers::settings_helper::add_ssl_client_opts`. That
function had no callers and the NRPE server defaults to `nrpe_dh_2048.pem`,
so nothing used it — but 512-bit DH is Logjam-broken, and an operator copying
the shipped default into `dh` inherited it. The file and the dead default are
gone.

**What to do:** if `dh` names `nrpe_dh_512.pem` explicitly, point it at
`${nrpe-dh}/nrpe_dh_2048.pem` before upgrading — the listener will otherwise
fail to start on a missing DH file.

#### Codec and protocol correctness

The same pass fixed a set of NRPE codec and protocol defects with no direct
security impact but which affect availability: the server parser did not
recognise wire version 4 (a v4 packet larger than the v2 packet length made
it drop the connection), `digest()` declared short packets complete before
they had arrived, three unsigned length underflows were guarded, a response
that could not be serialized left the protocol re-sending the same buffer,
and the client parsed its whole buffer rather than the bytes it received —
throwing the decoder's CRC error out through the asio event loop. See the
[Upgrading](../setup/upgrading.md) page for the operator-visible parts.
