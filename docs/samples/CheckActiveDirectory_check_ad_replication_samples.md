**Default check (healthy domain controller):**

```
check_ad_replication
OK: all 6 replication links are healthy|'DC02 DC=example,DC=com'=0;0;4 'DC02 CN=Configuration,DC=example,DC=com'=0;0;4 ...
```

**A partner has been failing for a while:**

```
check_ad_replication
CRITICAL: DC02 DC=example,DC=com: 7 failures, last success 2026-08-10 03:11:42|'DC02 DC=example,DC=com'=7;0;4 ...
```

**Only alert on prolonged outages (ignore single hiccups):**

```
check_ad_replication "warning=none" "critical=last_success < -24h"
OK: all 6 replication links are healthy
```

**Check a remote domain controller:**

```
check_ad_replication server=dc02.example.com
OK: all 6 replication links are healthy
```

**Custom output listing every link and its last error:**

```
check_ad_replication "top-syntax=${status}: ${list}" "detail-syntax=${source} -> ${naming_context}: ${last_error_message}"
WARNING: DC02 -> DC=example,DC=com: The RPC server is unavailable., DC03 -> DC=example,DC=com: 
```

**On a host that is not a domain controller (the fleet-wide-safe contract):**

```
check_ad_replication
Not a domain controller: Failed to bind to the directory service on WEB01: 6d9: There are no more endpoints available from the endpoint mapper.
```

**On the only domain controller of a single-DC domain:**

```
check_ad_replication
No replication partners found (single domain controller?)
```
