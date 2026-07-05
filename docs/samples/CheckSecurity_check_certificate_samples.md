#### Check a certificate file's expiry (default thresholds)

The default thresholds warn when a certificate expires within 30 days and go
critical within 10 days (matching common practice).

```
check_certificate file=/etc/ssl/certs/mysite.pem
L        cli OK: all 1 certificate(s) are ok
```

#### A certificate close to expiry trips the default critical

```
check_certificate file=/etc/ssl/certs/soon.pem
L        cli CRITICAL: /CN=soon.example.com expires in 5d (2026-07-09 19:16:12)
```

#### Custom thresholds and perfdata

`expires_in` is the number of whole days until expiry (negative once expired)
and is emitted as perfdata (unit `d`).

```
check_certificate file=/etc/ssl/certs/mysite.pem warning=expires_in<900
L        cli  Performance data: '/CN=valid.example.com'=825d;900;10
```

#### Scan a directory of certificates

```
check_certificate file=/etc/ssl/certs recursive=true "detail-syntax=${subject}: ${expires_in}d"
L        cli OK: all 137 certificate(s) are ok
```

#### Alert only on already-expired certificates

```
check_certificate file=/etc/pki/tls/certs critical=expired=1
L        cli OK: all 4 certificate(s) are ok
```

#### Flag weak keys or signatures (TLS hygiene)

```
check_certificate file=/etc/ssl/certs/mysite.pem "crit=weak_signature = 1 or weak_key = 1"
L        cli CRITICAL: /CN=legacy.example.com expires in 40d (2026-08-14 00:00:00)
```

Show the crypto detail for an audit:

```
check_certificate file=/etc/ssl/certs/mysite.pem "top-syntax=${list}" "detail-syntax=${subject}: ${signature_algorithm}, ${key_type}/${key_size}"
L        cli OK: /CN=mysite.example.com: sha256WithRSAEncryption, RSA/2048
```

#### Verify the certificate is trusted

`trusted` checks that the chain resolves to a trusted CA (time validity is
ignored — combine with `expired`). Point `ca=` at a bundle, or omit it to use the
system trust store:

```
check_certificate file=/etc/nginx/fullchain.pem "crit=not trusted or expired = 1"
check_certificate file=/etc/ssl/leaf.pem ca=/etc/ssl/corp-ca.pem "crit=not trusted"
```

#### Read a PKCS#12 (.pfx) file

```
check_certificate file=/opt/app/keystore.pfx password=changeit "crit=expires_in < 14"
L        cli OK: all 1 certificate(s) are ok
```

#### Windows certificate store (Windows only)

On Windows, `store=` enumerates a system certificate store; `location=` selects
`LocalMachine` (default) or `CurrentUser`.

```
check_certificate store=My location=LocalMachine
L        cli OK: all 6 certificate(s) are ok
```

```
check_certificate store=My "filter=subject like 'CN=*.example.com'" "crit=expires_in < 14"
L        cli WARNING: CN=web.example.com expires in 9d (2026-07-13 00:00:00)
```

On non-Windows platforms `store=` is rejected with a clear message:

```
check_certificate store=My
L        cli UNKNOWN: store= (certificate store) is only supported on Windows; use file= on this platform
```
