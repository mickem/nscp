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

