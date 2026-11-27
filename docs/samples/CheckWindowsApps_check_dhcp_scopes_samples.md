**Default check (warns above 80% used, critical above 95%, enabled scopes only):**

```
check_dhcp_scopes
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

**Include disabled scopes:**

```
check_dhcp_scopes "filter=none"
```

**List every scope, with the in-use, offered and total counts:**

```
check_dhcp_scopes show-all "detail-syntax=${scope} ${name} (${state}): ${in_use}/${addresses} in use, ${pending_offers} offered"
```
