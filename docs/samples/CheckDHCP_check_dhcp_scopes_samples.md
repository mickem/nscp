**Default check (all enabled scopes below 80% used):**

```
check_dhcp_scopes
OK: all 3 scopes are below their thresholds|'192.168.1.0/24_used_pct'=62%;80;95 '10.0.0.0/16_used_pct'=1%;80;95 '192.168.2.0/24_used_pct'=41%;80;95
```

**A scope is running out:**

```
check_dhcp_scopes
CRITICAL: 192.168.1.0/24 (Office): 85% used, 30 of 200 free, 192.168.2.0/24 (Guest WiFi): 98% used, 4 of 200 free|'192.168.1.0/24_used_pct'=85%;80;95 '10.0.0.0/16_used_pct'=1%;80;95 '192.168.2.0/24_used_pct'=98%;80;95
```

**Short thresholds on the used percentage:**

```
check_dhcp_scopes w=70 c=90
```

**Keep a fixed reserve of free addresses rather than a percentage:**

```
check_dhcp_scopes "warning=free < 50" "critical=free < 10"
```

**Only one scope, by name:**

```
check_dhcp_scopes "filter=name = 'Guest WiFi'"
```

**List every scope, with the in-use, offered and total counts:**

```
check_dhcp_scopes show-all "detail-syntax=${scope} ${name} (${state}): ${in_use}/${addresses} in use, ${pending_offers} offered"
CRITICAL: critical(192.168.2.0/24 Guest WiFi (enabled): 196/200 in use, 0 offered), warning(192.168.1.0/24 Office (enabled): 170/200 in use, 2 offered), 10.0.0.0/16 Lab (enabled): 10/1000 in use, 0 offered|...
```

**On a host without the DHCP Server role:**

```
check_dhcp_scopes
DHCP Server service is not running or not installed on this host: The RPC server is unavailable.
```
