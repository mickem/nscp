---
title: "NRPE and the shared TLS layer: key permissions, handshake deadline and codec fixes"
fixed_in: 0.18.1
severity: "High for the generated-key exposure, Low–Medium for the rest"
modules: [NRPEServer, NRPEClient, NSCAServer, NSClientServer, CheckMKServer, WEBServer, core]
action: conditional
---
A review of the NRPE path — wire codec, server parser and protocol, both
modules, and the shared socket/TLS layer under them. None of it is remotely
exploitable for code execution past `allowed hosts`.

- **Generated TLS private keys were world-readable.** `write_certs` used a
  plain `fopen`, so an unencrypted private key landed at `0644` under a normal
  systemd unit — and a default NRPE start generates that file when it is
  missing. Worse, the CA branch wrote the CA *private key* into the `ca.pem`
  operators are told to hand to clients, letting any recipient mint
  certificates that pass `verify mode = peer-cert`. Now `0600` (restricted
  DACL on Windows), with the CA key split out to `ca-key.pem`.
- **The inbound TLS handshake had no deadline.** The connection timer was
  armed only after the handshake completed, so a permitted host could open
  sockets, send nothing, and pin connections indefinitely. Now armed first.
- **The NRPE client never authenticated the server.** `verify mode` defaults
  to `none`, so `ssl = true` gave encryption with no authentication and
  undetectable on-path impersonation. The default is unchanged, but the client
  now logs it once per target, and generated certificates carry a usable SAN
  instead of localhost only.
- **A short v3/v4 packet let unchecksummed trailing bytes into the command.**
  The decoder bounded the payload by how many bytes arrived rather than by the
  declared length the CRC covers, so a small valid packet followed by ~1 KiB
  of arbitrary bytes had those bytes become the dispatched command.
- **512-bit DH parameters are no longer shipped.** `nrpe_dh_512.pem` was
  unused (the server defaults to 2048) but Logjam-broken, and an operator
  copying the shipped default inherited it. File and dead default removed.
- Smaller fixes: the peer certificate CN is constrained before it becomes a
  policy principal, `workarounds`/`single` no longer pollute the TLS verify
  mask, every documented `tls version` spelling is accepted, and a set of NRPE
  codec correctness bugs (wire version 4 unrecognised, premature packet
  completion, length underflows, a re-sending response loop).

**What to do:** the upgrade does not touch existing files. Check the
permissions of any certificate NSClient++ generated (`chmod 600
…/security/certificate.pem`), and if you distributed a generated `ca.pem`,
regenerate that CA and re-issue client certificates. If `dh` names
`nrpe_dh_512.pem` explicitly, point it at `${nrpe-dh}/nrpe_dh_2048.pem` before
upgrading.
