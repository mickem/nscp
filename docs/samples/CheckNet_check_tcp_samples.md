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

**Report who the certificate is for and who issued it:**

```
check_tcp host=secure.example.com port=443 ssl=true "top-syntax=${list}" "detail-syntax=cn=${cert_cn} issuer=${cert_issuer_cn} sans=${cert_sans}"
OK: cn=www.example.com issuer=R11 sans=DNS:example.com,DNS:www.example.com
```

**Alert when a renewal moves the certificate to a different CA:**

```
check_tcp host=secure.example.com port=443 ssl=true "crit=cert_issuer_cn != 'R11'" "top-syntax=${list}" "detail-syntax=issuer=${cert_issuer_cn}"
CRITICAL: issuer=E5
```

**Report why an untrusted chain did not verify (without refusing to connect):**

```
check_tcp host=internal.example.com port=443 ssl=true "top-syntax=${list}" "detail-syntax=verify=${cert_verify}"
OK: verify=unable to get local issuer certificate
```

**Require the names the certificate must cover (`sans=`):**

```
check_tcp host=secure.example.com port=443 ssl=true sans=example.com,www.example.com "top-syntax=${list}" "detail-syntax=${result} missing=[${missing_sans}]"
OK: ok missing=[]
```

```
check_tcp host=secure.example.com port=443 ssl=true sans=example.com,mail.example.com "top-syntax=${list}" "detail-syntax=${result} missing=[${missing_sans}]"
CRITICAL: san_missing missing=[mail.example.com]
```

**A required name with no certificate at all is still a missing name:**

```
check_http url=https://www.example.com sans=www.example.com onredirect=follow "detail-syntax=${result} missing=[${missing_sans}]"
CRITICAL: san_missing missing=[www.example.com]
```

**Check a virtual host reached by IP (`sni=` drives verification too):**

```
check_tcp host=10.0.0.5 port=443 ssl=true verify=peer ca=/etc/ssl/certs sni=www.example.com
OK: 10.0.0.5:443 ok in 24ms
```

**Check the certificate on a STARTTLS service (mail submission, LDAP, databases):**

```
check_tcp host=mail.example.com starttls=smtp "warn=ssl_expiry_days < 30" "top-syntax=${list}" "detail-syntax=${result} cert=${has_certificate} cn=${cert_cn}"
OK: ok cert=1 cn=mail.example.com
```

```
check_tcp host=db.example.com starttls=postgres "crit=ssl_expiry_days < 10" "top-syntax=${list}" "detail-syntax=${result} days=${ssl_expiry_days}"
OK: ok days=362
```

```
check_tcp host=ldap.example.com starttls=ldap "top-syntax=${list}" "detail-syntax=${result} cn=${cert_cn}"
OK: ok cn=ldap.example.com
```

**A server that declines the upgrade is reported, not waited out:**

```
check_tcp host=mail.example.com starttls=smtp "top-syntax=${list}" "detail-syntax=${result}"
CRITICAL: starttls_refused
```

**A peer that hangs up is a disconnect, not a timeout:**

```
check_tcp host=mail.example.com starttls=smtp "top-syntax=${list}" "detail-syntax=${result}"
CRITICAL: starttls_disconnected
```

**Verify against a hashed CA directory as well as a bundle file:**

```
check_tcp host=secure.example.com port=443 ssl=true verify=peer ca=/etc/ssl/certs
OK: secure.example.com:443 ok in 19ms
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_tcp --argument "host=192.168.56.1" --argument "port=22"
OK: All 1 hosts are ok|'192.168.56.1_22 time'=2ms;1000;5000
```

