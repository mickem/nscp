#### TLS certificate expiry (`ssl_expiry_days` / `has_certificate`)

When the connection is wrapped in TLS — `ssl=true`, or one of the implicit-TLS
service presets (`spop`, `simap`, `ssmtp`) — `check_tcp` reads the certificate
the peer serves and exposes it as two keywords:

| Keyword           | Type | Description                                                                     |
|-------------------|------|---------------------------------------------------------------------------------|
| `ssl_expiry_days` | int  | Whole days until the peer's certificate expires; **negative** once it has expired. `-1` when the connection is not TLS or the peer presented no certificate. Emitted as perfdata. |
| `has_certificate` | int  | `1` when the peer presented a certificate, `0` otherwise.                       |

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

**`-1` is ambiguous on its own, which is what `has_certificate` is for.** An
expired certificate legitimately reports a negative day count, so
`ssl_expiry_days = -1` could mean either "no certificate here" or "expired
yesterday". A bare `crit=ssl_expiry_days < 30` therefore also fires on every
plain connection. Guard it when that matters:

```
check_tcp host=mail.example.com port=993 ssl=true "crit=has_certificate = 1 and ssl_expiry_days < 30"
```

**Reading the certificate does not verify it.** The expiry is a property of what
the peer served, so it is available at the default `verify=none` — a
self-signed or otherwise untrusted certificate still reports its real remaining
lifetime. Use `verify=peer` with a `ca=` bundle when you want the chain checked
as well; the two are independent.

This complements the other two certificate checks: `check_http`'s
`ssl_expiry_days` covers HTTPS endpoints specifically, and `check_certificate`
inspects certificates at rest (files on disk, the Windows certificate store)
rather than ones served over a connection.
