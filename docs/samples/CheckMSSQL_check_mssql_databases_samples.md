**Default check (all databases healthy):**

```
check_mssql_databases
OK: All 4 databases are ONLINE
```

**Size perfdata per database (size keywords accept units):**

```
check_mssql_databases "warning=data_size > 100G"
OK: All 4 databases are ONLINE|'master_data'=4456448B;107374182400;0 'model_data'=8388608B;107374182400;0 'msdb_data'=15925248B;107374182400;0 'tempdb_data'=67108864B;107374182400;0
```

**Report state, recovery model and log usage for every database:**

```
check_mssql_databases "top-syntax=${status}: ${list}" "detail-syntax=${name}: ${state} ${recovery_model} log used ${log_used_pct}%" "warning=none" "critical=none" show-all
OK: appdb: ONLINE FULL log used 5%, master: ONLINE SIMPLE log used 30%, model: ONLINE FULL log used 12%, msdb: ONLINE SIMPLE log used 66%, tempdb: ONLINE SIMPLE log used 7%
```

**Alert on log usage in FULL-recovery databases:**

```
check_mssql_databases "filter=recovery_model = 'FULL'" "warning=log_used_pct > 80" "critical=log_used_pct > 90"
OK: All 2 databases are ONLINE|'appdb_log_used_pct'=6%;80;90 'model_log_used_pct'=12%;80;90
```

**Ignore an intentionally offline database:**

```
check_mssql_databases "filter=name != 'archive2019'"
OK: All 5 databases are ONLINE
```
