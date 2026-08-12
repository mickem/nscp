**Default check (healthy AG):**

```
check_mssql_availability_groups
OK: All 1 availability replicas/databases are healthy
```

**Data movement suspended (or any NOT_HEALTHY state) — critical by default:**

```
check_mssql_availability_groups
CRITICAL: 1/1 availability replicas/databases (ag1/f80785925e72/agdb: NOT_HEALTHY)
```

**Show role, connection and synchronization state of every replica/database:**

```
check_mssql_availability_groups "warning=none" "critical=health = 'NOT_HEALTHY'" "top-syntax=${status}: ${list}" "detail-syntax=${name}: ${role} ${connected_state} ${health} (${sync_state})"
OK: ag1/f80785925e72/agdb: PRIMARY CONNECTED HEALTHY (SYNCHRONIZED)
```

**Alert on replication lag before it breaks the RPO/RTO (size units):**

```
check_mssql_availability_groups "warning=redo_queue > 500M" "critical=log_send_queue > 1G"
OK: All 1 availability replicas/databases are healthy|'ag1/f80785925e72/agdb_log_send_queue'=0B;0;1073741824 'ag1/f80785925e72/agdb_redo_queue'=0B;524288000;0
```

**No availability groups configured — OK by default, so the check can be
deployed fleet-wide:**

```
check_mssql_availability_groups
OK: No availability groups found
```

**On a host where an AG must exist, make its absence page:**

```
check_mssql_availability_groups empty-state=critical
CRITICAL: No availability groups found
```
