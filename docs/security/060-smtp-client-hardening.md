---
title: "SMTP client security hardening"
fixed_in: 0.18.0
severity: "Low–Medium"
modules: [SMTPClient]
---
A review of the `SMTPClient` module produced a set of fixes to how it sets up
and trusts a submission session. None is remotely exploitable by an
unauthenticated third party on its own — the attacker positions required are a
man in the middle on the path to the mail server, or the ability to submit a
passive result the agent relays to a mail target — but each removes a way the
agent can be made to trust something it should not.

- **Data pipelined across the STARTTLS handshake is refused.** The client kept
  one receive buffer for the whole session, and `read_until()` consumes whole
  segments, so bytes appended to the server's `220` STARTTLS greeting stayed
  buffered across the handshake and were then served to every read that
  followed. Those bytes arrive in cleartext and are writable by anyone who can
  inject a packet, yet the rest of the session treated them as though they had
  come from inside the TLS tunnel — the STARTTLS command/response injection
  family ([CVE-2011-0411](https://nvd.nist.gov/vuln/detail/CVE-2011-0411) and
  relatives). Beyond forged capabilities after the upgrade, a prepared run of
  `2xx` replies walks the client through `MAIL`, `RCPT` and `DATA` and has it
  report a notification as delivered when nothing was ever sent, which for a
  monitoring agent means alerts that silently go nowhere. Per RFC 3207 §4 the
  session is now refused if anything is buffered at handshake time.
- **The EHLO name is validated before it reaches the wire.** The envelope
  addresses and the subject were all checked for CRLF injection; the EHLO
  argument was interpolated into the command line raw. It defaults to the
  submitting sender's host name, which for a relayed submission comes from the
  request header, so a CR or LF in it ended the `EHLO` and began a command of
  the sender's choosing on a session the agent had already authenticated —
  their `MAIL FROM` and `RCPT TO`, sent as the agent. Only a host name or an
  address literal is accepted now, and the check runs before any socket opens.
- **Server certificates are verified against the agent's CA bundle.**
  Verification rested on OpenSSL's compiled-in default verify paths, which on
  **Windows do not include the Windows certificate store**. The default
  `security=starttls` therefore failed verification against Gmail, Microsoft
  365 and every other public-CA submission service on every Windows agent, and
  the module's only escape hatch was `insecure-skip-verify` — so the
  configuration operators arrived at was the one with verification turned off.
  A `ca` setting now defaults to `${ca-path}` like every other TLS client in
  the tree. See the [upgrade note](../setup/upgrading.md#0180).
- **ESMTP capabilities are matched per line rather than by substring.** Reply
  lines were concatenated with no separator and searched with `find()`, so a
  server's free text could answer for a capability it never advertised — a
  greeting naming the host `starttls.example.com` satisfied the STARTTLS
  lookup, and the seam between two joined lines formed tokens no line
  contained. That lookup is what decides whether the session gets encrypted
  before `AUTH`.
- **The timeout bounds the whole submission.** Every operation took a fresh
  deadline, and connect took one per resolved address, so a peer answering
  just inside the deadline on each round trip — or a name resolving to several
  black-holed addresses — held a submission thread for a large multiple of the
  configured timeout. Name resolution ran outside the deadline entirely.
- **`insecure-skip-verify` no longer resets itself.** Declared as a
  `bool_switch`, its notifier fired with `false` on every submission that did
  not name the option, overwriting whatever the target had configured, so the
  setting never worked from configuration at all. It also rejected the valued
  `insecure-skip-verify=true` token that REST passes. The reset failed
  *towards* verifying, so this was not a hole — it is why the setting appeared
  to do nothing.
- **`--source-host` no longer redirects the connection.** Shared by every
  client module, it was registered against the destination container, where
  the well-known `host` key routes into the typed address field — so naming a
  source host pointed the client at that host instead of the configured
  server, sending the submission (credentials included, for a module that
  authenticates) somewhere the operator did not intend. It binds to the sender
  now. `SMTPClient` and `NRDPClient` were not exposed: each registered its own
  competing copy, which made the option ambiguous and refused outright.

**What to do:** nothing is required on unix, where `${ca-path}` resolves to the
distribution's own CA bundle. On **Windows**, an SMTP target that was working
only because `insecure-skip-verify = true` should have that removed and
retried — certificate verification now succeeds against public providers. A
target pointing at an internal relay with a private CA should name that bundle
in `ca` rather than waive verification. See
[Upgrading](../setup/upgrading.md#0180) for the full list.
