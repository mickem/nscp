---
title: "NRDP client security hardening"
fixed_in: 0.18.0
severity: "Low–Medium"
modules: [NRDPClient]
---
A focused hardening pass over the passive submission protocols (`NRDPClient`,
`NSCAClient`/`NSCAServer`) produced a small set of defense-in-depth fixes on
the NRDP side. None is a confirmed remote vulnerability in a default
configuration; they close latent gaps and bring NRDP in line with the
redaction already applied to NSCA.

- **A malformed NRDP server response can no longer crash the submitting
  agent.** `nrdp::data::parse_response` dereferenced the text node of the
  `<status>`/`<message>` elements without a null check, so a response
  containing an empty element (`<status></status>`) — which anyone able to
  answer or tamper with the connection can send, including a man-in-the-middle
  on a plaintext or unverified link — dereferenced a null pointer and crashed
  the process rather than throwing. It is now reported as an invalid response.
- **The NRDP token is no longer written to the trace log.** The per-target
  configuration is emitted at trace level on every submission and included the
  `token` — a shared secret equivalent to the NSCA password, which was already
  redacted. It now logs `<set>`/`<unset>`, and any credentials embedded in a
  configured `proxy` URL (`http://user:pass@host`) are redacted to
  `<redacted>@host`.
- **HTTPS submissions no longer silently skip certificate verification when no
  verify mode is set.** An unset `verify mode` resolves to `verify_none` in the
  TLS layer. Configured targets already defaulted to `peer`, but the bare
  `nscp client`/REST submission path did not, so an `https://` submission made
  that way validated neither the certificate chain nor the host name and
  exposed the token to a man-in-the-middle. That path now defaults to `peer`
  as well; plain `http://` is unaffected.

**What to do:** nothing required for the default install or for targets
configured through settings. If you submit to an `https://` NRDP endpoint via
`nscp client`/REST against a self-signed certificate *without* a configured
verify mode, add `--verify none` (or a CA) explicitly — the previous
behaviour was to trust any certificate silently.
