**Default check (healthy — nothing open):**

```
check_mssql_transactions
OK: No open transactions
```

**Default check with a leaked transaction — the idle 5-minute fuse fires while
an equally old but actively working transaction stays quiet:**

```
check_mssql_transactions
WARNING: 1/2 open transactions (session 54 (appdb/sa) open for 349s (idle: 1))|'54_transaction_age'=349s;1800;7200 '55_transaction_age'=349s;1800;7200
```

**List everything with full detail (idle flag, request age and command):**

```
check_mssql_transactions "warning=none" "critical=transaction_age > 4h" "top-syntax=${status}: ${list}" "detail-syntax=session ${session_id} (${database}/${login}): ${transaction_name} open ${transaction_age}s, idle=${is_idle}, request=${request_age}s ${command}"
OK: session 54 (appdb/sa): user_transaction open 349s, idle=1, request=-1s , session 55 (master/sa): user_transaction open 349s, idle=0, request=349s WAITFOR|'54_transaction_age'=349s;0;14400 '55_transaction_age'=349s;0;14400
```

Session 54 is the leak (open transaction, no active request); session 55 is a
long-running but working request.

**Tighter thresholds (time units):**

```
check_mssql_transactions "warning=transaction_age > 2m or is_idle = 1 and transaction_age > 1m" "critical=transaction_age > 2h"
WARNING: 2/2 open transactions (session 54 (appdb/sa) open for 349s (idle: 1), session 55 (master/sa) open for 349s (idle: 0))|'54_transaction_age'=349s;120;7200 '55_transaction_age'=349s;120;7200
```

**Page only on leaked transactions:**

```
check_mssql_transactions "warning=none" "critical=is_idle = 1 and transaction_age > 1m" "detail-syntax=${database}/${login} session ${session_id} idle in transaction for ${transaction_age}s"
CRITICAL: 1/2 open transactions (appdb/sa session 54 idle in transaction for 349s)|'54_transaction_age'=349s;0;60 '55_transaction_age'=349s;0;60
```
