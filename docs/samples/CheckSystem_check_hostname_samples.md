**Default check (identity line):**

```
check_hostname
OK: WEB01 (web01.corp.example.com), domain=corp.example.com
```

On a workgroup machine:

```
check_hostname
OK: MYPC (MyPC), workgroup=WORKGROUP
```

**Require domain membership (CRITICAL on domain-join / workgroup drift):**

```
check_hostname "crit=join != 'domain'"
CRITICAL: MYPC (MyPC), workgroup=WORKGROUP

check_hostname "crit=join != 'domain' or domain != 'corp.example.com'"
OK: WEB01 (web01.corp.example.com), domain=corp.example.com
```

**Detect name drift (FQDN or NetBIOS out of sync):**

```
check_hostname "warn=fqdn_consistent = 0 or netbios_matches_dns = 0"
OK: WEB01 (web01.corp.example.com), domain=corp.example.com
```

**Pin the expected hostname (cloned or re-imaged box detection):**

```
check_hostname "crit=hostname != 'WEB01'"
OK: WEB01 (web01.corp.example.com), domain=corp.example.com
```

**Inspect all identity fields:**

```
check_hostname "detail-syntax=nb=${hostname} dns=${dns_hostname} dom=${domain} fq=${fqdn} ok=${fqdn_consistent}/${netbios_matches_dns}"
OK: nb=WEB01 dns=web01 dom=corp.example.com fq=web01.corp.example.com ok=1/1
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_hostname --argument "crit=join != 'domain'"
OK: WEB01 (web01.corp.example.com), domain=corp.example.com
```
