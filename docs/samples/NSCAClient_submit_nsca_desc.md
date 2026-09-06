#### About `submit_nsca`

`submit_nsca` submits a passive check result to an **NSCA** daemon — the classic
Nagios passive-result transport.

The usual way to use it is to route results rather than call it by hand: give a
scheduled check `target=nsca`, or add `NSCA` to the channels a check reports on,
and each result is submitted as it is produced. `NSCA` is the default channel
for [`check_and_forward`](../check/CheckHelpers.md#check_and_forward). A direct
call is mainly useful for verifying that the password, cipher and payload length
match the receiving daemon.

##### Three settings must match the server exactly

NSCA has no negotiation: the client and the daemon must agree up front, and a
mismatch shows up as a silently dropped or garbled result rather than an error.

- **`encryption`** — the cipher, defaulting to `aes`. It must equal the
  `decryption_method` in the daemon's `nsca.cfg`.
- **`password`** — the shared secret, matching the daemon's `password`.
- **`payload length`** — 512 by default, matching NSCA's compiled-in
  `MAX_PACKETSIZE`. A daemon built with a larger payload needs the same value
  here, and a mismatch truncates or corrupts every message.

If results simply never appear on the server, check these three before anything
else.

##### What NSCA's "encryption" is and is not

The NSCA ciphers are a shared-secret obfuscation over a plain TCP connection.
There is no server authentication, no replay protection and no integrity
guarantee, and `encryption = none` disables even the obfuscation. It should not
be relied on across an untrusted network.

This module can additionally wrap the connection in **TLS** — `certificate`,
`certificate key`, `ca`, `dh` and `allowed ciphers` — which is what actually
authenticates the peer. Where the receiving side supports it, prefer that, or
consider [NSCA-ng](NSCANgClient.md) instead, which was designed with TLS from
the start.

`encoding` sets the character encoding used for the message text, which matters
when check output contains non-ASCII characters.
