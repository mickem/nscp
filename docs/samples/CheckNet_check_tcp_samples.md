**Default check against a single host/port:**

```
check_tcp host=127.0.0.1 port=8443
L        cli OK: OK: 127.0.0.1:8443 ok in 0ms
L        cli  Performance data: '127.0.0.1_8443_time'=0;1000;5000
```

**Multiple hosts via comma list:**

```
check_tcp host=www.google.com host=www.ibm.com port=443 timeout=2000
L        cli OK: OK: www.google.com:443 ok in 11ms, www.ibm.com:443 ok in 9ms
L        cli  Performance data: 'www.google.com_443_time'=11;1000;5000 'www.ibm.com_443_time'=9;1000;5000
```

**Send a payload and require an expected substring in the response:**

```
check_tcp host=smtp.gmail.com port=25 send="EHLO nsclient.org" expect="250"
L        cli CRITICAL: CRITICAL: smtp.gmail.com:25 no_match in 25ms
L        cli  Performance data: 'smtp.gmail.com_25_time'=25;1000;5000
```

**Tighter thresholds:**

```
check_tcp host=www.google.com port=443 "warn=time > 100" "crit=time > 500 or connected = 0"
L        cli OK: OK: www.google.com:443 ok in 11ms
L        cli  Performance data: 'www.google.com_443_connected'=1;0;0 'www.google.com_443_time'=11;100;500
```

**Show every host explicitly:**

```
check_tcp host=a.example.com host=b.example.com port=80 "top-syntax=%(status): %(list)" "detail-syntax=%(host):%(port)=%(result) in %(time)ms"
OK: a.example.com:80=ok in 14ms, b.example.com:80=ok in 19ms
```

**Use a service preset (`ftp`, `pop`, `imap`, `smtp`, `ssh`) — sets the port, greeting and expected-response regex:**

```
check_tcp host=mail.example.com service=smtp
OK: mail.example.com:25 ok in 8ms
```

**Wrap the connection in TLS with `ssl=true` (e.g. to test an HTTPS listener answers):**

```
check_tcp host=www.google.com port=443 ssl=true
OK: www.google.com:443 ok in 11ms|'www.google.com_443_time'=11;1000;5000
```

**Implicit-TLS service presets (`spop`, `simap`, `ssmtp`) connect over TLS and check the greeting:**

```
check_tcp host=smtp.gmail.com service=ssmtp
OK: smtp.gmail.com:465 ok in 16ms|'smtp.gmail.com_465_time'=16;1000;5000
```

**Match the peer's response with a regex via the `response` keyword:**

```
check_tcp host=mail.example.com port=25 "crit=response not regexp '^220'"
OK: mail.example.com:25 ok in 8ms
```

**Check how long the peer's TLS certificate is still valid (`ssl_expiry_days`):**

```
check_tcp host=secure.example.com port=443 ssl=true "warn=ssl_expiry_days < 30" "crit=ssl_expiry_days < 10" "top-syntax=${list}" "detail-syntax=cert expires in ${ssl_expiry_days} days"
OK: cert expires in 399 days|'secure.example.com_443_ssl_expiry_days'=399;30;10
```

```
check_tcp host=expiring.example.com port=443 ssl=true "warn=ssl_expiry_days < 30" "crit=ssl_expiry_days < 10" "top-syntax=${list}" "detail-syntax=cert expires in ${ssl_expiry_days} days"
WARNING: cert expires in 19 days|'expiring.example.com_443_ssl_expiry_days'=19;30;10
```

**A plain connection cannot trip the expiry threshold — and can be tested for explicitly:**

```
check_tcp host=mail.example.com port=110 "warn=none" "crit=ssl_expiry_days < 30"
OK: mail.example.com:110 ok in 1ms
```

```
check_tcp host=mail.example.com port=110 "warn=none" "crit=ssl_expiry_days = 'no certificate'"
CRITICAL: mail.example.com:110 ok in 0ms
```

**The certificate keywords also work through the implicit-TLS presets:**

```
check_tcp host=imap.example.com service=simap "top-syntax=${list}" "detail-syntax=${host}:${port} cert=${has_certificate} days=${ssl_expiry_days}"
OK: imap.example.com:993 cert=1 days=399
```

**Without TLS there is no certificate at all:**

```
check_tcp host=mail.example.com port=110 "top-syntax=${list}" "detail-syntax=cert=${has_certificate} days=${ssl_expiry_days}"
OK: cert=0 days=no certificate
```

**Verify the server certificate when using TLS (needs a CA bundle):**

```
check_tcp host=secure.example.com port=443 ssl=true verify=peer ca=/etc/ssl/certs/ca-certificates.crt
OK: secure.example.com:443 ok in 21ms
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_tcp --argument "host=192.168.56.1" --argument "port=22"
OK: All 1 hosts are ok|'192.168.56.1_22 time'=2ms;1000;5000
```

