#### TLS certificate expiry (`ssl_expiry_days` / `has_certificate`)

When the connection is wrapped in TLS — `ssl=true`, or one of the implicit-TLS
service presets (`spop`, `simap`, `ssmtp`) — `check_tcp` reads the certificate
the peer serves and exposes it as two keywords:

| Keyword           | Type | Description                                                                     |
|-------------------|------|---------------------------------------------------------------------------------|
| `ssl_expiry_days` | int  | Whole days until the peer's certificate expires; **negative** once it has expired. Renders as **`no certificate`** when the connection is not TLS or the peer presented none. Emitted as perfdata (only when a certificate exists). |
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

**Reading the certificate does not verify it.** The expiry is a property of what
the peer served, so it is available at the default `verify=none` — a
self-signed or otherwise untrusted certificate still reports its real remaining
lifetime. Use `verify=peer` with a `ca=` bundle when you want the chain checked
as well; the two are independent.

This complements the other two certificate checks: `check_http`'s
`ssl_expiry_days` covers HTTPS endpoints specifically, and `check_certificate`
inspects certificates at rest (files on disk, the Windows certificate store)
rather than ones served over a connection.
