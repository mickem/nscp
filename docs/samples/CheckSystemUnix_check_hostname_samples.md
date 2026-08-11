**Default check (identity line):**

```
check_hostname
OK: web01 (web01.corp.example.com), domain=corp.example.com
```

On a host without DNS (e.g. a container), the FQDN falls back to the hostname:

```
check_hostname
OK: container123 (container123), domain=
```

**Pin the expected identity (re-imaged or cloned box detection):**

```
check_hostname "crit=hostname != 'web01' or domain != 'corp.example.com'"
OK: web01 (web01.corp.example.com), domain=corp.example.com
```

**Detect DNS drift (the resolver canonicalises this host under another name):**

```
check_hostname "warn=fqdn_consistent = 0"
WARNING: web01 (web01-old.corp.example.com), domain=corp.example.com
```

**Inspect all identity fields:**

```
check_hostname "detail-syntax=h=${hostname} f=${fqdn} d=${domain} ok=${fqdn_consistent}"
OK: h=web01 f=web01.corp.example.com d=corp.example.com ok=1
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_hostname --argument "crit=domain != 'corp.example.com'"
OK: web01 (web01.corp.example.com), domain=corp.example.com
```
