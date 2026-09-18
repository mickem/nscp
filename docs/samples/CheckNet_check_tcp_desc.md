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
check_tcp host=ldap.example.com port=636 ssl=true "warn=ssl_expiry_days < 30" "crit=ssl_expiry_days < 10"
```

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
check_tcp host=mail.example.com port=993 ssl=true "crit=ssl_expiry_days < 30 or ssl_expiry_days = 'no certificate'"
```

> **Upgrading.** `ssl_expiry_days` used to report `-1` for a connection with no
> certificate, which made a bare `crit=ssl_expiry_days < 30` fire on every plain
> connection. That sentinel is gone: filters written as `ssl_expiry_days = -1`
> must become `ssl_expiry_days = 'no certificate'` (or use `has_certificate`),
> and no expiry perfdata is emitted when there is no certificate. The same
> change applies to `check_http`'s `ssl_expiry_days`.

**Reading the certificate does not verify it.** The expiry is a property of what
the peer served, so it is available at the default `verify=none` — a
self-signed or otherwise untrusted certificate still reports its real remaining
lifetime. Use `verify=peer` with a `ca=` bundle when you want the chain checked
as well; the two are independent.

This complements the other two certificate checks: `check_http`'s
`ssl_expiry_days` covers HTTPS endpoints specifically, and `check_certificate`
inspects certificates at rest (files on disk, the Windows certificate store)
rather than ones served over a connection.

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
check_tcp host=secure.example.com port=443 ssl=true "crit=cert_issuer_cn != 'R11'"
```

`cert_verify` is recorded **even at the default `verify=none`**: OpenSSL walks
the chain regardless, it just does not fail the handshake over the result. That
is what lets a check report *why* a chain is untrusted without refusing to
connect. It is not an authentication result on its own — only a successful
handshake under `verify=peer` is that.

#### Requiring names with `sans=`

`sans=` takes a comma separated list of names the certificate must cover
through subjectAltName. A name that is missing sets `result` to `san_missing`
— which the default `critical` filter (`result != 'ok'`) already alerts on —
and lists what was missing in `missing_sans`:

```
check_tcp host=secure.example.com port=443 ssl=true sans=example.com,www.example.com
```

Matching follows RFC 6125: a `*.example.com` entry covers `www.example.com`
but **not** `example.com` itself, and not `a.b.example.com`. That is the
mistake worth checking for — a wildcard certificate that does not cover the
apex. Asking for a literal `*.example.com` requires the wildcard entry itself,
which is how you assert that a certificate really is a wildcard.

Names are matched against subjectAltName only, never the subject CN: a name
carried only by the CN has not been a valid identity since RFC 2818 was
superseded, and no current browser or library accepts it.

#### `sni=` — checking a virtual host

`sni=` sets the Server Name Indication offered to a server that hosts several
certificates, **and** the name the certificate is verified against. It defaults
to the host connected to, so it is only needed when those differ — reaching a
virtual host by IP, or checking one name on a shared listener:

```
check_tcp host=10.0.0.5 port=443 ssl=true verify=peer ca=/etc/ssl/certs sni=www.example.com
```

Because it drives verification too, a name the certificate does not carry fails
the handshake rather than quietly skipping the check.

#### Opportunistic TLS with `starttls=`

Not every TLS service has an implicit-TLS port. Mail submission (587), LDAP,
PostgreSQL and MySQL all start in the clear and upgrade on request, so their
certificates are unreachable with `ssl=true` alone. `starttls=` performs the
protocol's own upgrade negotiation first, then checks the certificate exactly
as usual:

```
check_tcp host=mail.example.com starttls=smtp "warn=ssl_expiry_days < 30"
check_tcp host=ldap.example.com starttls=ldap "crit=ssl_expiry_days < 10"
check_tcp host=db.example.com   starttls=postgres sans=db.example.com
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
presets are for.

A server that declines the upgrade reports `result=starttls_refused` rather
than waiting out the timeout, which the default `critical` filter alerts on.
For MySQL that includes a server that never advertised `CLIENT_SSL` in its
handshake, so "this server has TLS turned off" reads as a refusal rather than a
handshake failure.

#### CA bundles and CA directories

`ca=` accepts either a concatenated PEM bundle file or a hashed CA *directory*
in OpenSSL's `-CApath` layout. `/etc/ssl/certs` is a directory on every
distribution, so both of these work:

```
check_tcp host=secure.example.com port=443 ssl=true verify=peer ca=/etc/ssl/certs/ca-certificates.crt
check_tcp host=secure.example.com port=443 ssl=true verify=peer ca=/etc/ssl/certs
```
