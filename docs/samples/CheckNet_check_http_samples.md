**Default check against a single URL (success):**

```
check_http url=https://nsclient.org/
L        cli OK: : https://nsclient.org/ -> 200 ok (61204B in 561ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=561;5000;0
```

**HTTPS with explicit port and path components (page not found):**

```
check_http protocol=https host=nsclient.org port=443 path=/no-such-page
L        cli CRITICAL: : https://nsclient.org:443/no-such-page -> 404 http_404 (46098B in 163ms)
L        cli  Performance data: 'https://nsclient.org:443/no-such-page_code'=404;0;200 'https://nsclient.org:443/no-such-page_time'=163;5000;0
```

**Connection / DNS failure (host does not resolve):**

```
check_http url=https://nope.invalid/
L        cli CRITICAL: : https://nope.invalid/ -> 0 error: resolve: Ingen sådan värd är känd (0B in 0ms)
L        cli  Performance data: 'https://nope.invalid/_code'=0;0;200 'https://nope.invalid/_time'=0;5000;0
```

**Multiple URLs in one call:**

```
check_http url=https://nsclient.org/ url=https://nsclient.org/nsclient/
L        cli OK: : https://nsclient.org/ -> 200 ok (61204B in 39ms), https://nsclient.org/nsclient/ -> 200 ok (50656B in 160ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=39;5000;0 'https://nsclient.org/nsclient/_code'=200;0;200 'https://nsclient.org/nsclient/_time'=160;5000;0
```

**Require an expected substring in the response body:**

```
check_http url=https://nsclient.org/ expected-body="NSClient"
L        cli OK: : https://nsclient.org/ -> 200 ok (61204B in 37ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=37;5000;0
```

If the substring is absent the check goes CRITICAL with `result=no_match`:

```
check_http url=https://nsclient.org/ expected-body="this-string-is-not-present"
L        cli CRITICAL: : https://nsclient.org/ -> 200 no_match (61204B in 34ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=34;5000;0
```

**Custom user-agent and extra headers:**

```
check_http url=https://nsclient.org/ user-agent="nscp-monitor/1" header="X-Trace: 1"
L        cli OK: : https://nsclient.org/ -> 200 ok (61204B in 34ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=34;5000;0
```

**Tighter latency thresholds and code rules:**

```
check_http url=https://nsclient.org/ timeout=10000 "warn=time > 500 or code >= 400" "crit=time > 2000 or code >= 500 or result != 'ok'"
L        cli OK: : https://nsclient.org/ -> 200 ok (61204B in 36ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;400;500 'https://nsclient.org/_time'=36;500;2000
```

**Custom output text (drop result if you don't want it):**

```
check_http url=https://nsclient.org/ "top-syntax=%(status): %(list)" "detail-syntax=%(url) -> %(code) in %(time)ms"
L        cli OK: : https://nsclient.org/ -> 200 in 55ms
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=55;5000;0
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_http --argument "url=https://nsclient.org/"
OK: https://nsclient.org/ -> 200 ok (61204B in 561ms)| 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=561;5000;0
```
