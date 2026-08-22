**Check a local NGINX via its stub_status endpoint:**

```
check_nginx_status url=http://127.0.0.1/nginx_status
OK: ok: 291 active (6 reading, 179 writing, 106 waiting)|'127.0.0.1_active'=291;0;0
```

**Alert when connections pile up:**

```
check_nginx_status url=http://127.0.0.1/nginx_status "warning=active > 200" "critical=active > 400"
WARNING: ok: 291 active (6 reading, 179 writing, 106 waiting)|'127.0.0.1_active'=291;200;400
```

**Alert when NGINX has started dropping connections (accepted minus handled):**

```
check_nginx_status url=http://127.0.0.1/nginx_status "warning=dropped > 0"
WARNING: ok: 291 active (6 reading, 179 writing, 106 waiting)|'127.0.0.1_active'=291;0;0 '127.0.0.1_dropped'=2c;0;0
```

**A server that is down is CRITICAL by default:**

```
check_nginx_status url=http://127.0.0.1:81/nginx_status
CRITICAL: error: Failed to connect to 127.0.0.1:81: No connection could be made because the target machine actively refused it: 0 active (0 reading, 0 writing, 0 waiting)|'127.0.0.1_active'=0;0;0
```
