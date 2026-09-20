**Default check against a single URL (success):**

```
check_http url=https://nsclient.org/
L        cli OK: https://nsclient.org/ -> 200 ok (68937B in 197ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=197ms;5000;0
```

**HTTPS with explicit port and path components (page not found):**

```
check_http protocol=https host=nsclient.org port=443 path=/no-such-page
L        cli CRITICAL: https://nsclient.org:443/no-such-page -> 404 http_404 (55109B in 165ms)
L        cli  Performance data: 'https://nsclient.org:443/no-such-page_code'=404;0;200 'https://nsclient.org:443/no-such-page_size'=55109B;0;0 'https://nsclient.org:443/no-such-page'=165ms;5000;0
```

**Connection / DNS failure (host does not resolve):**

```
check_http url=https://nope.invalid/
L        cli CRITICAL: https://nope.invalid/ -> 0 error: Failed to resolve nope.invalid:443: Ingen sådan värd är känd (0B in 10ms)
L        cli  Performance data: 'https://nope.invalid/_code'=0;0;200 'https://nope.invalid/_size'=0B;0;0 'https://nope.invalid/'=10ms;5000;0
```

**Multiple URLs in one call:**

```
check_http url=https://nsclient.org/ url=https://nsclient.org/nsclient/
L        cli OK: https://nsclient.org/ -> 200 ok (68937B in 59ms), https://nsclient.org/nsclient/ -> 200 ok (60820B in 179ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=59ms;5000;0 'https://nsclient.org/nsclient/_code'=200;0;200 'https://nsclient.org/nsclient/_size'=60820B;0;0 'https://nsclient.org/nsclient/'=179ms;5000;0
```

**Require an expected substring in the response body:**

```
check_http url=https://nsclient.org/ expected-body="NSClient"
L        cli OK: https://nsclient.org/ -> 200 ok (68937B in 47ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=47ms;5000;0
```

If the substring is absent the check goes CRITICAL with `result=no_match`:

```
check_http url=https://nsclient.org/ expected-body="this-string-is-not-present"
L        cli CRITICAL: https://nsclient.org/ -> 200 no_match (68937B in 52ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=52ms;5000;0
```

**Custom user-agent and extra headers:**

```
check_http url=https://nsclient.org/ user-agent="nscp-monitor/1" header="X-Trace: 1"
L        cli OK: https://nsclient.org/ -> 200 ok (68937B in 50ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=50ms;5000;0
```

**Tighter latency thresholds and code rules:**

```
check_http url=https://nsclient.org/ timeout=10000 "warn=time > 500 or code >= 400" "crit=time > 2000 or code >= 500 or result != 'ok'"
L        cli OK: https://nsclient.org/ -> 200 ok (68937B in 61ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;400;500 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=61ms;500;2000
```

**Custom output text (drop result if you don't want it):**

```
check_http url=https://nsclient.org/ "top-syntax=%(status): %(list)" "detail-syntax=%(url) -> %(code) in %(time)ms"
L        cli OK: https://nsclient.org/ -> 200 in 46ms
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=46ms;5000;0
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_http --argument "url=https://nsclient.org/"
OK: https://nsclient.org/ -> 200 ok (68937B in 197ms)|'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=197ms;5000;0
```

**Use a specific HTTP method (`HEAD`, `POST`, `PUT`, …):**

```
check_http url=https://www.google.com method=HEAD
OK: https://www.google.com -> 200 ok (0B in 58ms)|'https://www.google.com_code'=200;0;200 'https://www.google.com_size'=0B;0;0 'https://www.google.com'=58ms;5000;0
```

**POST a body (`post-data` implies POST unless `method=` is given):**

```
check_http url=https://httpbin.org/post post-data="name=value" content-type="application/x-www-form-urlencoded" expected-body="name"
OK: https://httpbin.org/post -> 200 ok (429B in 380ms)
```

**HTTP Basic authentication:**

```
check_http url=https://example.com/private username=admin password=secret
OK: https://example.com/private -> 200 ok (1200B in 88ms)
```

**Follow redirects (default reports the 3xx as-is; `onredirect=follow` chases the Location):**

```
check_http url=http://github.com onredirect=follow "detail-syntax=${url} ${result} code=${code}"
OK: http://github.com ok code=200|'http://github.com_code'=200;0;200 'http://github.com_size'=575830B;0;0 'http://github.com'=119ms;5000;0
```

**Accept a set of status codes with the `code` keyword, and match the body with a regex:**

`regexp` is a **full** match, not a search, so a body pattern has to cover the
whole document — `'.*Welcome.*'`, not `'Welcome'`. For a plain substring reach
for `expected-body=` instead: it sets `result=no_match`, which the default
`critical` filter already alerts on and the default `detail-syntax` already
shows.

```
check_http url=https://nsclient.org/ "warn=code not in (200,301,302)" "crit=code >= 500 or body not regexp '.*NSClient.*'" "detail-syntax=${url} ${result} code=${code} (${size}B in ${time}ms)"
OK: https://nsclient.org/ ok code=200 (80939B in 100ms)|'https://nsclient.org/_code'=200;0;500 'https://nsclient.org/_size'=80939B;0;0
```

**Alert when the TLS certificate is about to expire (`ssl_expiry_days`):**

```
check_http url=https://www.google.com "warn=ssl_expiry_days < 30" "crit=ssl_expiry_days < 7" "detail-syntax=${url} cert expires in ${ssl_expiry_days} days"
OK: https://www.google.com cert expires in 67 days|'https://www.google.com_size'=84168B;0;0 'https://www.google.com_ssl_expiry_days'=67;30;7
```

**Report the certificate's identity, not just its expiry:**

```
check_http url=https://www.google.com "detail-syntax=${url} cn=${cert_cn} issuer=${cert_issuer_cn} sans=${cert_sans}"
OK: https://www.google.com cn=www.google.com issuer=WR2 sans=DNS:www.google.com|'https://www.google.com_code'=200;0;200 'https://www.google.com_size'=84080B;0;0 'https://www.google.com'=113ms;5000;0
```

**Require the names the certificate must cover (`sans=`):**

```
check_http url=https://www.google.com sans=www.google.com "detail-syntax=${url} ${result} code=${code} missing=[${missing_sans}]"
OK: https://www.google.com ok code=200 missing=[]|'https://www.google.com_code'=200;0;200 'https://www.google.com_size'=84198B;0;0 'https://www.google.com'=91ms;5000;0
```

```
check_http url=https://www.google.com sans=mail.google.com "detail-syntax=${url} ${result} code=${code} missing=[${missing_sans}]"
CRITICAL: https://www.google.com san_missing code=200 missing=[mail.google.com]|'https://www.google.com_code'=200;0;200 'https://www.google.com_size'=84445B;0;0 'https://www.google.com'=123ms;5000;0
```

**A required name with no certificate at all is still a missing name:**

`sans=` is evaluated against the certificate of the hop actually checked, so an
https URL that redirects down to plain http ends on a hop that served none —
and covers no names:

```
check_http url=https://www.example.com sans=www.example.com onredirect=follow "detail-syntax=${url} ${result} code=${code} missing=[${missing_sans}]"
CRITICAL: http://www.example.com/ san_missing code=200 missing=[www.example.com]
```

**Report why a chain did not verify:**

```
check_http url=https://internal.example.com verify=none "detail-syntax=${url} ${result} verify=${cert_verify}"
OK: https://internal.example.com ok verify=unable to get local issuer certificate
```
