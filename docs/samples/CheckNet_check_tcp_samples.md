**Default check against a single host/port:**

```
check_tcp host=127.0.0.1 port=8443
OK: 127.0.0.1:8443 ok in 0ms|'127.0.0.1_8443'=0ms;1000;5000
```

**Several hosts in one call (`host=` repeated, or `hosts=` as a comma list):**

```
check_tcp hosts=www.google.com,www.ibm.com port=443 timeout=2000
OK: www.google.com:443 ok in 9ms, www.ibm.com:443 ok in 10ms|'www.google.com_443'=9ms;1000;5000 'www.ibm.com_443'=10ms;1000;5000
```

**Send a payload and require an expected substring in the response:**

Render `${response}` as well, or a failed match reports `no_match` without
showing what the peer actually said:

```
check_tcp host=smtp.gmail.com port=25 send="EHLO nsclient.org" expect="250" "detail-syntax=${host}:${port} ${result} in ${time}ms got=[${response}]"
CRITICAL: smtp.gmail.com:25 no_match in 18ms got=[220 smtp.gmail.com ESMTP 4fb4d7f45d1cf-6aa67dc582csm2423438a12.16 - gsmtp]|'smtp.gmail.com_25'=18ms;1000;5000
```

**Tighter thresholds:**

```
check_tcp host=www.google.com port=443 "warn=time > 100" "crit=time > 500 or connected = 0"
OK: www.google.com:443 ok in 5ms|'www.google.com_443_connected'=1;0;0 'www.google.com_443'=5ms;100;500
```

**Show every host explicitly:**

```
check_tcp host=www.google.com host=www.ibm.com port=443 "top-syntax=%(status): %(list)" "detail-syntax=%(host):%(port)=%(result) in %(time)ms"
OK: www.google.com:443=ok in 13ms, www.ibm.com:443=ok in 11ms|'www.google.com_443'=13ms;1000;5000 'www.ibm.com_443'=11ms;1000;5000
```

**Use a service preset (`ftp`, `pop`, `imap`, `smtp`, `ssh`) — sets the port, greeting and expected-response regex:**

```
check_tcp host=smtp.gmail.com service=smtp
OK: smtp.gmail.com:25 ok in 14ms|'smtp.gmail.com_25'=14ms;1000;5000
```

**Wrap the connection in TLS with `ssl=true` (e.g. to test an HTTPS listener answers):**

```
check_tcp host=www.google.com port=443 ssl=true
OK: www.google.com:443 ok in 10ms|'www.google.com_443'=10ms;1000;5000
```

**Implicit-TLS service presets (`spop`, `simap`, `ssmtp`) connect over TLS and check the greeting:**

```
check_tcp host=smtp.gmail.com service=ssmtp
OK: smtp.gmail.com:465 ok in 18ms|'smtp.gmail.com_465'=18ms;1000;5000
```

**Match the peer's response with a regex via the `response` keyword:**

`regexp` is a **full** match, not a search, so the pattern has to cover the
whole greeting — `'220.*'`, not `'^220'` (use `like` for a plain substring):

```
check_tcp host=smtp.gmail.com service=ssmtp "crit=response not regexp '220.*'" "detail-syntax=${host}:${port} ${result} greeting=[${response}]"
OK: smtp.gmail.com:465 ok greeting=[220 smtp.gmail.com ESMTP a640c23a62f3a-c2a3523809fsm167937966b.5 - gsmtp]|'smtp.gmail.com_465'=17ms;1000;0
```

> **Render what you filtered on.** The default `detail-syntax` is
> `${host}:${port} ${result} in ${time}ms`, and `result` is the *connection*
> outcome — so a check that goes CRITICAL on a certificate or a greeting still
> reads `ok in 10ms` and never says why. The filter decides the state, the
> syntax decides the text, and they share nothing. Every example below therefore
> pairs its `warn=` / `crit=` with a `detail-syntax` that renders the keyword the
> threshold is about.

**Check how long the peer's TLS certificate is still valid (`ssl_expiry_days`):**

```
check_tcp host=www.google.com port=443 ssl=true "warn=ssl_expiry_days < 30" "crit=ssl_expiry_days < 10" "detail-syntax=${host}:${port} cert expires in ${ssl_expiry_days} days"
OK: www.google.com:443 cert expires in 67 days|'www.google.com_443_ssl_expiry_days'=67;30;10
```

```
check_tcp host=expiring.example.com port=443 ssl=true "warn=ssl_expiry_days < 30" "crit=ssl_expiry_days < 10" "detail-syntax=${host}:${port} cert expires in ${ssl_expiry_days} days"
WARNING: expiring.example.com:443 cert expires in 19 days|'expiring.example.com_443_ssl_expiry_days'=19;30;10
```

**A plain connection cannot trip the expiry threshold — and can be tested for explicitly:**

```
check_tcp host=www.google.com port=80 "warn=none" "crit=ssl_expiry_days < 30" "detail-syntax=${host}:${port} ${result} cert=${has_certificate} days=${ssl_expiry_days}"
OK: www.google.com:80 ok cert=0 days=no certificate
```

```
check_tcp host=www.google.com port=80 "warn=none" "crit=ssl_expiry_days = 'no certificate'" "detail-syntax=${host}:${port} ${result} cert=${has_certificate} days=${ssl_expiry_days}"
CRITICAL: www.google.com:80 ok cert=0 days=no certificate
```

`result` stays `ok` in the second one — the TCP connection really was fine, and
it is the certificate keywords in the detail line that show what the `critical`
filter matched on.

**The certificate keywords also work through the implicit-TLS presets:**

```
check_tcp host=smtp.gmail.com service=ssmtp "detail-syntax=${host}:${port} cert=${has_certificate} days=${ssl_expiry_days} cn=${cert_cn}"
OK: smtp.gmail.com:465 cert=1 days=67 cn=smtp.gmail.com|'smtp.gmail.com_465'=14ms;1000;5000
```

**Without TLS there is no certificate at all:**

```
check_tcp host=www.google.com port=80 "detail-syntax=${host}:${port} cert=${has_certificate} days=${ssl_expiry_days}"
OK: www.google.com:80 cert=0 days=no certificate|'www.google.com_80'=10ms;1000;5000
```

**Verify the server certificate when using TLS (needs a CA bundle):**

`cert_verify` carries OpenSSL's verdict, so the detail line says whether the
chain was accepted rather than only that the connection worked:

```
check_tcp host=www.google.com port=443 ssl=true verify=peer ca=/etc/ssl/certs/ca-certificates.crt "detail-syntax=${host}:${port} ${result} verify=${cert_verify} in ${time}ms"
OK: www.google.com:443 ok verify=ok in 5ms|'www.google.com_443'=5ms;1000;5000
```

**Report who the certificate is for and who issued it:**

```
check_tcp host=www.google.com port=443 ssl=true "detail-syntax=${host}:${port} cn=${cert_cn} issuer=${cert_issuer_cn} sans=${cert_sans}"
OK: www.google.com:443 cn=www.google.com issuer=WR2 sans=DNS:www.google.com|'www.google.com_443'=8ms;1000;5000
```

**Alert when a renewal moves the certificate to a different CA:**

```
check_tcp host=www.google.com port=443 ssl=true "crit=cert_issuer_cn != 'WE2'" "detail-syntax=${host}:${port} issuer=${cert_issuer_cn}"
CRITICAL: www.google.com:443 issuer=WR2|'www.google.com_443'=22ms;1000;0
```

**A certificate that does not verify fails the check** — verification is the
default, against the agent's own trust bundle:

```
check_tcp host=secure.example.com port=443 ssl=true "detail-syntax=${host}:${port} ${result} verify=${cert_verify}"
CRITICAL: secure.example.com:443 tls_handshake_failed verify=self-signed certificate|'secure.example.com_443'=0ms;1000;5000
```

**Report why a chain did not verify, without refusing to connect** — `cert_verify`
is recorded at `verify=none` too, which is how you watch a service whose
certificate you do not trust:

```
check_tcp host=secure.example.com port=443 ssl=true verify=none "detail-syntax=${host}:${port} ${result} verify=${cert_verify}"
OK: secure.example.com:443 ok verify=self-signed certificate|'secure.example.com_443'=5ms;1000;5000
```

**The certificate is reported even when the handshake failed**, so an expiry
threshold still fires on the certificate that was rejected:

```
check_tcp host=secure.example.com port=443 ssl=true "detail-syntax=${host}:${port} ${result} cn=${cert_cn} days=${ssl_expiry_days} verify=${cert_verify}"
CRITICAL: secure.example.com:443 tls_handshake_failed cn=secure.example.com days=89 verify=self-signed certificate|'secure.example.com_443'=0ms;1000;5000
```

**Trust an internal CA with `ca=`:**

```
check_tcp host=secure.example.com port=443 ssl=true ca=/etc/pki/internal-ca.pem "detail-syntax=${host}:${port} ${result} verify=${cert_verify}"
OK: secure.example.com:443 ok verify=ok|'secure.example.com_443'=2ms;1000;5000
```

**Require the names the certificate must cover (`sans=`):**

```
check_tcp host=www.google.com port=443 ssl=true sans=www.google.com "detail-syntax=${host}:${port} ${result} missing=[${missing_sans}]"
OK: www.google.com:443 ok missing=[]|'www.google.com_443'=7ms;1000;5000
```

```
check_tcp host=www.google.com port=443 ssl=true sans=mail.google.com "detail-syntax=${host}:${port} ${result} missing=[${missing_sans}]"
CRITICAL: www.google.com:443 san_missing missing=[mail.google.com]|'www.google.com_443'=5ms;1000;5000
```

**Check a virtual host reached by IP (`sni=` drives verification too):**

```
check_tcp host=142.251.156.119 port=443 ssl=true verify=peer ca=/etc/ssl/certs sni=www.google.com "detail-syntax=${host}:${port} ${result} cn=${cert_cn} verify=${cert_verify} in ${time}ms"
OK: 142.251.156.119:443 ok cn=www.google.com verify=ok in 4ms|'142.251.156.119_443'=4ms;1000;5000
```

Without `sni=` the same connection is verified against the literal IP, which the
certificate does not carry — and `cert_verify` is what says so:

```
check_tcp host=142.251.156.119 port=443 ssl=true verify=peer ca=/etc/ssl/certs "detail-syntax=${host}:${port} ${result} verify=${cert_verify} in ${time}ms"
CRITICAL: 142.251.156.119:443 tls_handshake_failed verify=unspecified certificate verification error in 4ms|'142.251.156.119_443'=4ms;1000;5000
```

**Check the certificate on a STARTTLS service (mail submission, LDAP, databases):**

```
check_tcp host=smtp.gmail.com starttls=smtp "warn=ssl_expiry_days < 30" "detail-syntax=${host}:${port} ${result} cert=${has_certificate} cn=${cert_cn} days=${ssl_expiry_days}"
OK: smtp.gmail.com:25 ok cert=1 cn=smtp.gmail.com days=67|'smtp.gmail.com_25_ssl_expiry_days'=67;30;0 'smtp.gmail.com_25'=14ms;0;5000
```

```
check_tcp host=db.example.com starttls=postgres "crit=ssl_expiry_days < 10" "detail-syntax=${host}:${port} ${result} days=${ssl_expiry_days}"
OK: db.example.com:5432 ok days=362|'db.example.com_5432_ssl_expiry_days'=362;0;10
```

```
check_tcp host=ldap.example.com starttls=ldap "detail-syntax=${host}:${port} ${result} cn=${cert_cn}"
OK: ldap.example.com:389 ok cn=ldap.example.com|'ldap.example.com_389'=6ms;1000;5000
```

**A negotiation that never reaches TLS is reported through `result`, not waited out:**

```
check_tcp host=mail.example.com starttls=smtp "detail-syntax=${host}:${port} ${result}"
CRITICAL: mail.example.com:25 starttls_refused|'mail.example.com_25'=12ms;1000;5000
```

```
check_tcp host=mail.example.com starttls=smtp "detail-syntax=${host}:${port} ${result}"
CRITICAL: mail.example.com:25 starttls_disconnected|'mail.example.com_25'=9ms;1000;5000
```

**Verify against a hashed CA directory as well as a bundle file:**

```
check_tcp host=secure.example.com port=443 ssl=true verify=peer ca=/etc/ssl/certs "detail-syntax=${host}:${port} ${result} verify=${cert_verify} in ${time}ms"
OK: secure.example.com:443 ok verify=ok in 19ms|'secure.example.com_443'=19ms;1000;5000
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_tcp --argument "host=192.168.56.1" --argument "port=22"
OK: 192.168.56.1:22 ok in 2ms|'192.168.56.1_22'=2ms;1000;5000
```
