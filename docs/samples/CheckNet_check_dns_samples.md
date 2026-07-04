**Default lookup of a hostname:**

```
check_dns host=google.com
check_dns host=google.com
L        cli OK: OK: google.com -> 172.217.20.174 (1) in 10ms [ok]
L        cli  Performance data: 'google.com_time'=10;1000;0
```

**Lookup with a custom timeout:**

```
check_dns host=google.com timeout=2000
check_dns host=google.com timeout=2000
L        cli OK: OK: google.com -> 172.217.20.174 (1) in 3ms [ok]
L        cli  Performance data: 'google.com_time'=3;1000;0
```

**Verify the resolver returns specific addresses:**

```
check_dns host=google.com expected-address=172.217.20.174
L        cli OK: OK: google.com -> 172.217.20.174 (1) in 3ms [ok]
L        cli  Performance data: 'google.com_time'=3;1000;0
```

**Verify against multiple expected addresses (comma list):**

```
check_dns host=google.com "expected=93.184.216.34,2606:2800:220:1:248:1893:25c8:1946"
L        cli CRITICAL: CRITICAL: google.com -> 172.217.20.174 (1) in 7ms [mismatch]
L        cli  Performance data: 'google.com_time'=7;1000;0
```

**Tighter latency thresholds:**

```
check_dns host=nsclient.org "warn=time > 100" "crit=time > 5 or result != 'ok'"
L        cli CRITICAL: CRITICAL: nsclient.org -> 188.114.97.1,188.114.96.1 (2) in 8ms [ok]
L        cli  Performance data: 'nsclient.org_time'=8;100;5
```

**Custom output text:**

```
check_dns host=google.com "top-syntax=%(status): %(list)" "detail-syntax=%(host)=%(addresses) [%(result)]"
L        cli OK: OK: google.com=172.217.20.174 [ok]
L        cli  Performance data: 'google.com_time'=5;1000;0
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_dns --argument "host=example.com"
OK: google.com -> 172.217.20.174 (1) in 10ms [ok]|'google.com_time'=10;1000;0
```

**Query a specific record type (`type=A|AAAA|MX|TXT|CNAME|NS|SOA|PTR`):**

```
check_dns host=google.com type=MX server=8.8.8.8
OK: google.com -> 10 smtp.google.com (1) in 9ms [ok]|'google.com_time'=9;1000;0
```

**Query a specific DNS server (A/AAAA without `server=` use the system resolver; any other type or an explicit `server=` uses a direct DNS-over-UDP query):**

```
check_dns host=nsclient.org type=TXT server=1.1.1.1
OK: nsclient.org -> v=spf1 include:_spf.google.com ~all (1) in 12ms [ok]
```

**Non-recursive query against an authoritative server on a custom port:**

```
check_dns host=example.com type=A server=192.168.10.53 port=5353 norecursion=true
OK: example.com -> 93.184.216.34 (1) in 3ms [ok]
```

