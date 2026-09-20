#### TLS certificate expiry (`ssl_expiry_days` / `has_certificate`)

When the connection is wrapped in TLS — `ssl=true`, or one of the implicit-TLS
service presets (`spop`, `simap`, `ssmtp`) — `check_tcp` reads the certificate
the peer serves and exposes it as two keywords: `ssl_expiry_days`, the whole
days until the certificate expires (**negative** once it has expired), and
`has_certificate`, `1` when the peer presented one.

This makes certificate monitoring work for any TLS service, not just HTTPS —
LDAPS, IMAPS, SMTPS, RDP, a database listener, or anything else that speaks TLS
on a port:

```
check_tcp host=ldap.example.com port=636 ssl=true "warn=ssl_expiry_days < 30" "crit=ssl_expiry_days < 10" "detail-syntax=${host}:${port} cert expires in ${ssl_expiry_days} days"
```

The `detail-syntax` is not decoration. The default one renders
`${host}:${port} ${result} in ${time}ms`, and `result` is the *connection*
outcome — so without it a certificate threshold flips the check to CRITICAL
while the message still reads `ok in 10ms` and never names the certificate.
Render the keyword you threshold on, and the alert explains itself.

Two details worth knowing.

**The count is truncated, not rounded.** A certificate with 23 hours left reads
as `0`, not `1` — the remainder is dropped rather than rounded up into a
reassuring number.

**A missing certificate is not a number.** `ssl_expiry_days` is an *optional
number*: with no certificate it renders as `no certificate`, every numeric
comparison on it is false, and no perfdata is emitted. A bare
`crit=ssl_expiry_days < 30` is therefore safe — it cannot fire on a plain
connection, while an expired certificate still reports its real (negative) day
count and fires as it should. Test for the no-certificate state explicitly with
the string form, or with `has_certificate`:

```
check_tcp host=mail.example.com port=993 ssl=true "crit=ssl_expiry_days < 30 or ssl_expiry_days = 'no certificate'" "detail-syntax=${host}:${port} ${result} cert=${has_certificate} days=${ssl_expiry_days}"
```

> **Upgrading.** `ssl_expiry_days` used to report `-1` for a connection with no
> certificate, which made a bare `crit=ssl_expiry_days < 30` fire on every plain
> connection. That sentinel is gone: filters written as `ssl_expiry_days = -1`
> must become `ssl_expiry_days = 'no certificate'` (or use `has_certificate`),
> and no expiry perfdata is emitted when there is no certificate. The same
> change applies to `check_http`'s `ssl_expiry_days`.

**The certificate is verified by default.** `verify` defaults to `peer` and
`ca=` defaults to the agent's own trust bundle (`${ca-path}`), falling back to
the trust store OpenSSL was built with when that is empty — so a check against
a publicly issued certificate validates out of the box, and one against a
certificate that does not validate reports `tls_handshake_failed`.

**Reading the certificate does not require verifying it.** The expiry and the
identity keywords are properties of what the peer served, so they are readable
without a trust decision: pass `verify=none` to reach a server whose
certificate does not validate and still report its real remaining lifetime,
issuer and names. `cert_verify` tells you why it did not validate either way.

```
check_tcp host=internal.example.com port=443 ssl=true verify=none "detail-syntax=${host} days=${ssl_expiry_days} verify=${cert_verify}"
```

This complements the other two certificate checks: `check_http`'s
`ssl_expiry_days` covers HTTPS endpoints specifically, and `check_certificate`
inspects certificates at rest (files on disk, the Windows certificate store)
rather than ones served over a connection.

> `ssl_expiry_days` also renders `no certificate` for a certificate that *was*
> served but whose `notAfter` could not be read. Reporting that as a day count
> would let `crit=ssl_expiry_days < 1` fire on a parse failure; `has_certificate`
> is what tells the two apart.

#### Certificate identity (`cert_cn`, `cert_sans`, `cert_issuer_cn`, …)

Alongside the expiry, `check_tcp` reports who the certificate is for and who
issued it, so a check can alert on a certificate being *replaced* as well as on
it running out.

| Keyword | Value |
| --- | --- |
| `cert_subject` | Subject as an RFC 2253 string, e.g. `CN=www.example.com,O=Acme` |
| `cert_cn` | The subject's commonName only, e.g. `www.example.com` |
| `cert_issuer` | Issuer as an RFC 2253 string |
| `cert_issuer_cn` | The issuer's commonName, e.g. `R11` |
| `cert_sans` | subjectAltName entries, comma separated, in the openssl form (`DNS:host`, `IP:addr`) |
| `cert_self_signed` | True when subject equals issuer |
| `cert_verify` | OpenSSL's verdict on the chain: `ok`, or why it did not verify |
| `missing_sans` | Names required via `sans=` that the certificate does not cover |

`cert_cn` is empty for a certificate that identifies its hosts only through
subjectAltName, which is normal and not an error — assert on `cert_sans` (or
the `sans=` option below) instead. Alerting on an unexpected issuer catches a
renewal that silently moved to a different CA:

```
check_tcp host=secure.example.com port=443 ssl=true "crit=cert_issuer_cn != 'R11'" "detail-syntax=${host}:${port} issuer=${cert_issuer_cn}"
```

`cert_verify` is recorded **even at `verify=none`**: OpenSSL walks the chain
regardless, it just does not fail the handshake over the result. That is what
lets a check report *why* a chain is untrusted without refusing to connect. It
is not an authentication result on its own — only a successful handshake under
`verify=peer` is that.

When the handshake **fails**, `cert_verify` carries the chain's verdict only if
that verdict is itself a failure — which is the case worth reading, since it is
then the reason the handshake failed. It is left empty when the connection died
before any chain was checked (a reset, a timeout, a rejected TLS version),
because OpenSSL reports `X509_V_OK` for "never verified anything" as well as
for "verified fine". So `crit=cert_verify != 'ok'` fires on both a bad chain
and a connection that never got far enough to check one, and never reads a
clean chain into a failure that had nothing to do with certificates.

**The certificate is reported even when the handshake failed.** A rejected
certificate is exactly the one worth looking at, so `cert_cn`, `cert_sans`,
`ssl_expiry_days` and the rest are filled in alongside
`result=tls_handshake_failed`. An expiry threshold therefore still fires on a
certificate that expired under `verify=peer`, rather than going quiet because
the connection was refused over it.

#### Requiring names with `sans=`

`sans=` takes a comma separated list of names the certificate must cover
through subjectAltName. A name that is missing sets `result` to `san_missing`
— which the default `critical` filter (`result != 'ok'`) already alerts on —
and lists what was missing in `missing_sans`:

```
check_tcp host=secure.example.com port=443 ssl=true sans=example.com,www.example.com "detail-syntax=${host}:${port} ${result} missing=[${missing_sans}]"
```

Matching follows RFC 6125: a `*.example.com` entry covers `www.example.com`
but **not** `example.com` itself, and not `a.b.example.com`. That is the
mistake worth checking for — a wildcard certificate that does not cover the
apex. Asking for a literal `*.example.com` requires the wildcard entry itself,
which is how you assert that a certificate really is a wildcard.

Names are matched against subjectAltName only, never the subject CN: a name
carried only by the CN has not been a valid identity since RFC 2818 was
superseded, and no current browser or library accepts it.

A connection that served **no** certificate does not cover a required name
either, so `sans=` on a plain connection reports `san_missing` rather than
`ok`. That is what forgetting `starttls=` on a submission port looks like, and
reporting it as a pass would hide exactly the case the option exists to catch.

#### `sni=` — checking a virtual host

`sni=` sets the Server Name Indication offered to a server that hosts several
certificates, **and** the name the certificate is verified against. It defaults
to the host connected to, so it is only needed when those differ — reaching a
virtual host by IP, or checking one name on a shared listener:

```
check_tcp host=10.0.0.5 port=443 ssl=true verify=peer ca=/etc/ssl/certs sni=www.example.com "detail-syntax=${host}:${port} ${result} cn=${cert_cn} verify=${cert_verify}"
```

Because it drives verification too, a name the certificate does not carry fails
the handshake rather than quietly skipping the check.

`sni=` only means something inside a TLS session, so it is **rejected** when
neither `ssl=true`, a `starttls=` protocol nor an implicit-TLS `service=`
preset is in play. Accepting it silently would return OK having asserted
nothing — which reads exactly like a passing identity check.

#### How a check can end (`result`)

`result` is a short status word, and the default `critical` filter is
`result != 'ok'` — so every value but `ok` alerts without a threshold being
written for it.

| `result` | Meaning |
| --- | --- |
| `ok` | Connected, and the expectations (if any) held |
| `refused` | The port refused the connection |
| `timeout` | The TCP connect did not complete inside `timeout=` |
| `resolve_failed` | The host did not resolve (in the requested address family) |
| `no_match` | Connected, but the response failed `expect=` / the preset's pattern |
| `read_timeout` | The peer stayed silent past `timeout=` |
| `read_failed` | The read failed for another reason |
| `write_timeout` | The peer never accepted `send=` inside `timeout=` |
| `write_failed` | The write failed for another reason |
| `san_missing` | A name required with `sans=` is not covered — see below |
| `tls_handshake_failed` | The TLS handshake was rejected (bad chain, wrong name, no shared version) |
| `tls_handshake_timeout` | The handshake did not complete inside `timeout=` |
| `starttls_*` | The opportunistic upgrade did not happen — see the table below |

A timeout is kept distinct from the failure it is easy to confuse it with: a
peer that went quiet (`read_timeout`) is not one that hung up (`read_failed`),
and closing the socket to unblock the read makes both look identical at the
error code, so the deadline is what tells them apart.

#### Opportunistic TLS with `starttls=`

Not every TLS service has an implicit-TLS port. Mail submission (587), LDAP,
PostgreSQL and MySQL all start in the clear and upgrade on request, so their
certificates are unreachable with `ssl=true` alone. `starttls=` performs the
protocol's own upgrade negotiation first, then checks the certificate exactly
as usual:

```
check_tcp host=mail.example.com starttls=smtp "warn=ssl_expiry_days < 30" "detail-syntax=${host}:${port} ${result} days=${ssl_expiry_days}"
check_tcp host=ldap.example.com starttls=ldap "crit=ssl_expiry_days < 10" "detail-syntax=${host}:${port} ${result} days=${ssl_expiry_days}"
check_tcp host=db.example.com   starttls=postgres sans=db.example.com "detail-syntax=${host}:${port} ${result} missing=[${missing_sans}]"
```

Supported protocols, with the plaintext port each defaults to:

| `starttls=` | Default port | Upgrade |
| --- | --- | --- |
| `smtp` | 25 | `EHLO` then `STARTTLS` |
| `lmtp` | 24 | `LHLO` then `STARTTLS` |
| `pop3` | 110 | `STLS` |
| `imap` | 143 | `a001 STARTTLS` |
| `ftp` | 21 | `AUTH TLS` |
| `nntp` | 119 | `STARTTLS` |
| `sieve` | 4190 | `STARTTLS` |
| `irc` | 6667 | `STARTTLS` (numeric 670) |
| `postgres` | 5432 | SSLRequest packet |
| `mysql` | 3306 | SSLRequest packet |
| `ldap` | 389 | StartTLS extended request (1.3.6.1.4.1.1466.20037) |

`starttls=` implies `ssl=true` and sets the default port, so `host=` alone is
usually enough. The port defaulted to is always the **cleartext** one (143, not
993) — the implicit-TLS ports are what the `simap` / `spop` / `ssmtp` service
presets are for. For the same reason `starttls=` cannot be combined with a
`service=` preset: the preset waits for a greeting that is not sent again after
the upgrade, so the two together would sit out the timeout. The check says so
rather than failing mysteriously.

How a negotiation can end:

| `result` | Meaning |
| --- | --- |
| `ok` | The upgrade succeeded and the certificate was read |
| `starttls_refused` | The server answered, declining the upgrade |
| `starttls_disconnected` | The peer closed or reset the connection mid-negotiation |
| `starttls_timeout` | No answer inside `timeout=` |
| `starttls_overflow` | The peer sent more than 64 KiB the negotiation could not consume |
| `tls_handshake_failed` | The server agreed, but the TLS handshake itself failed |

All but `ok` trip the default `critical` filter (`result != 'ok'`). A refusal
is an *answer*, so it is reported immediately rather than waited out; for MySQL
that includes a server that never advertised `CLIENT_SSL` in its handshake, so
"this server has TLS turned off" reads as a refusal rather than a handshake
failure. `starttls_overflow` is kept distinct from `starttls_timeout` because
the budget that ran out is bytes rather than milliseconds — raising `timeout=`
cannot help there. It is the answer for a peer whose reply never *ends*: the
line engine consumes each complete line as it arrives, so a server chattering
endless complete lines is bounded by `timeout=` as usual, while one that never
sends the terminating newline (or floods one of the binary protocols) would
otherwise grow the buffer without limit.

#### CA bundles and CA directories

`ca=` defaults to the agent's configured bundle (`${ca-path}`) and accepts
either a concatenated PEM bundle file or a hashed CA *directory* in OpenSSL's
`-CApath` layout. `/etc/ssl/certs` is a directory on every distribution, so
both of these work:

```
check_tcp host=secure.example.com port=443 ssl=true verify=peer ca=/etc/ssl/certs/ca-certificates.crt "detail-syntax=${host}:${port} ${result} verify=${cert_verify}"
check_tcp host=secure.example.com port=443 ssl=true verify=peer ca=/etc/ssl/certs "detail-syntax=${host}:${port} ${result} verify=${cert_verify}"
```
