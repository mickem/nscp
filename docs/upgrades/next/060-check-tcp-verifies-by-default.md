---
icon: "🔒 💥"
modules: [CheckNet]
action: conditional
---
**`check_tcp` and `check_ssh` now verify the server certificate by default.**
`verify` defaulted to `none`, so a TLS check completed its handshake against
any certificate at all and reported `ok`. It now defaults to `peer`, and `ca=`
defaults to the agent's own trust bundle (`${ca-path}`, the same one
`check_http` has always used), falling back to the trust store OpenSSL was
built with when that is empty. Checks against publicly issued certificates
start validating out of the box; checks against a self-signed or internal
certificate that used to report `ok` now report `tls_handshake_failed`.

- **If you check an internal or self-signed service**, either point `ca=` at
  the issuing CA — a PEM bundle or a hashed directory both work — or add
  `verify=none` to keep the previous behaviour. The certificate keywords
  (`ssl_expiry_days`, `cert_cn`, `cert_sans`, `cert_verify`, …) are readable
  either way: they are properties of what the peer served, not a trust
  decision.
- **Only TLS checks are affected**, which means `ssl=true`, a `starttls=`
  protocol, or one of the implicit-TLS `service=` presets (`spop`, `simap`,
  `ssmtp`). A plain `check_tcp` is unchanged.
- **`sni=` is now rejected** when the connection is not TLS, instead of being
  silently ignored. It named the certificate to ask for and to verify against,
  so without TLS it asserted nothing while still reporting `ok`.
- **`sans=` on a plain connection now reports `san_missing`** rather than `ok`.
  A connection that served no certificate covers no required name — which is
  what forgetting `starttls=` on a submission port looks like.

Both commands are marked **experimental**, which is the flag that reserves
exactly this: their options, keywords and output may still change while the
shape of the check settles.

See the
[security notice](../security/notices.md#check_tcp-completed-tls-handshakes-without-verifying-the-server-certificate).
