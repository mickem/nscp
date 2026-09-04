**Default check (identity line):**

```
check_hostname
OK: WEB01 (web01.corp.example.com), domain=corp.example.com
```

On a Windows workgroup machine:

```
check_hostname
OK: MYPC (MyPC), workgroup=WORKGROUP
```

On a Linux host without DNS (e.g. a container), the FQDN falls back to the hostname:

```
check_hostname
OK: container123 (container123), domain=
```

**Pin the expected identity (cloned or re-imaged box detection):**

```
check_hostname "crit=hostname != 'web01' or domain != 'corp.example.com'"
OK: web01 (web01.corp.example.com), domain=corp.example.com
```

**Detect name drift (the resolver canonicalises this host under another name):**

```
check_hostname "warn=fqdn_consistent = 0"
WARNING: web01 (web01-old.corp.example.com), domain=corp.example.com
```

**Require domain membership — Windows only (CRITICAL on domain-join / workgroup drift):**

```
check_hostname "crit=join != 'domain'"
CRITICAL: MYPC (MyPC), workgroup=WORKGROUP

check_hostname "crit=join != 'domain' or domain != 'corp.example.com'"
OK: WEB01 (web01.corp.example.com), domain=corp.example.com
```

**Detect NetBIOS drift — Windows only:**

```
check_hostname "warn=fqdn_consistent = 0 or netbios_matches_dns = 0"
OK: WEB01 (web01.corp.example.com), domain=corp.example.com
```

**Inspect all identity fields:**

```
check_hostname "detail-syntax=h=${hostname} f=${fqdn} d=${domain} ok=${fqdn_consistent}"
OK: h=web01 f=web01.corp.example.com d=corp.example.com ok=1
```

On Windows the NetBIOS and DNS names are separate fields:

```
check_hostname "detail-syntax=nb=${hostname} dns=${dns_hostname} dom=${domain} fq=${fqdn} ok=${fqdn_consistent}/${netbios_matches_dns}"
OK: nb=WEB01 dns=web01 dom=corp.example.com fq=web01.corp.example.com ok=1/1
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_hostname --argument "crit=domain != 'corp.example.com'"
OK: web01 (web01.corp.example.com), domain=corp.example.com
```
