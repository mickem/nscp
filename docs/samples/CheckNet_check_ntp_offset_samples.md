**Default check against a single NTP server:**

```
check_ntp_offset server=pool.ntp.org
L        cli OK: OK: pool.ntp.org offset=1326ms stratum=2
L        cli  Performance data: 'pool.ntp.org_offset'=1326;60000;120000 'pool.ntp.org_stratum'=2;16;16
```

**Multiple servers via comma list (averaged across answers):**

```
check_ntp_offset "servers=0.pool.ntp.org,1.pool.ntp.org,2.pool.ntp.org" timeout=2000
L        cli OK: OK: 0.pool.ntp.org offset=1326ms stratum=2, 1.pool.ntp.org offset=1327ms stratum=1, 2.pool.ntp.org offset=1329ms stratum=2
L        cli  Performance data: '0.pool.ntp.org_offset'=1326;60000;120000 '0.pool.ntp.org_stratum'=2;16;16 '1.pool.ntp.org_offset'=1327;60000;120000 '1.pool.ntp.org_stratum'=1;16;16 '2.pool.ntp.org_offset'=1329;60000;120000 '2.pool.ntp.org_stratum'=2;16;16
```

**Custom port and timeout:**

```
check_ntp_offset server=time.example.com port=123 timeout=1500
check_ntp_offset server=time.example.com port=123 timeout=1500
L        cli OK: OK: time.example.com offset=0ms stratum=0
L        cli  Performance data: 'time.example.com_offset'=0;60000;120000 'time.example.com_stratum'=0;16;16
```

**Tighter thresholds (alert when more than 50ms / 200ms off):**

```
check_ntp_offset server=pool.ntp.org "warn=offset > 50 or stratum >= 8" "crit=offset > 200 or stratum >= 16"
L        cli CRITICAL: CRITICAL: pool.ntp.org offset=1326ms stratum=1
L        cli  Performance data: 'pool.ntp.org_offset'=1326;50;200 'pool.ntp.org_stratum'=1;8;16
```

**Use signed offset to distinguish ahead vs behind:**

```
check_ntp_offset server=pool.ntp.org "top-syntax=%(status): %(list)" "detail-syntax=%(server) signed=%(offset_signed)ms abs=%(offset)ms s=%(stratum)"
L        cli OK: OK: pool.ntp.org signed=1327ms abs=1327ms s=1
L        cli  Performance data: 'pool.ntp.org_offset'=1327;60000;120000 'pool.ntp.org_stratum'=1;16;16
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_ntp_offset --argument "server=pool.ntp.org"
OK: pool.ntp.org offset=1326ms stratum=2| 'pool.ntp.org_offset'=1326;60000;120000 'pool.ntp.org_stratum'=2;16;16
```


**Measure jitter across a burst of samples (needs `samples` >= 2):**

```
check_ntp_offset server=ntp.example.com samples=6 "warn=jitter > 50" "crit=jitter > 100" "top-syntax=${list}" "detail-syntax=${server} jitter=${jitter}ms over ${samples} samples"
WARNING: ntp.example.com jitter=70ms over 6 samples|'ntp.example.com_jitter'=70ms;50;100
```

**Alert on an inaccurate clock and an unstable source independently:**

```
check_ntp_offset server=ntp.example.com samples=6 "warn=offset > 100 or jitter > 50" "crit=offset > 1000 or jitter > 200 or stratum >= 16" "top-syntax=${list}" "detail-syntax=offset=${offset_signed}ms jitter=${jitter}ms"
WARNING: offset=35ms jitter=70ms|'ntp.example.com_jitter'=70ms;50;200
```

**Report what the server claims about its own accuracy (no extra traffic):**

```
check_ntp_offset server=ntp.example.com "top-syntax=${list}" "detail-syntax=${server} root_delay=${root_delay}ms root_dispersion=${root_dispersion}ms stratum=${stratum}"
OK: ntp.example.com root_delay=11ms root_dispersion=33ms stratum=2
```
