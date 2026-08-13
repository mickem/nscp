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

**Alert before a file hits its max_size or fills its volume (`>= 0` excludes
the `-1` unknown sentinel):**

```
check_mssql_databases "warning=data_headroom < 1K and data_headroom >= 0" "critical=log_headroom < 1K and log_headroom >= 0"
OK: All 5 databases are ONLINE|'appdb_data_headroom'=96468992B;1024;0 'appdb_log_headroom'=991888719872B;0;1024 'master_data_headroom'=991888719872B;1024;0 'master_log_headroom'=991888719872B;0;1024 'model_data_headroom'=991888719872B;1024;0 'model_log_headroom'=991888719872B;0;1024 'msdb_data_headroom'=991888719872B;1024;0 'msdb_log_headroom'=991888719872B;0;1024 'tempdb_data_headroom'=991888719872B;1024;0 'tempdb_log_headroom'=991888719872B;0;1024
```

Headroom is whichever runs out first, the file's `max_size` cap or the volume it
grows into. `appdb` here is capped at 100MB with ~92MB left, so its cap binds;
everything else is bounded by the ~924GB free on the volume — including the log
files, which carry the engine's 2TB cap but cannot reach it on this disk.

**A database approaching its size cap trips the threshold:**

```
check_mssql_databases "warning=data_headroom < 200M and data_headroom >= 0" "critical=none" "top-syntax=${status}: ${list}" "detail-syntax=${name}: data headroom ${data_headroom}B, log headroom ${log_headroom}B"
WARNING: appdb: data headroom 96468992B, log headroom 991888719872B, master: data headroom 991888719872B, log headroom 991888719872B, model: data headroom 991888719872B, log headroom 991888719872B, msdb: data headroom 991888719872B, log headroom 991888719872B, tempdb: data headroom 991888719872B, log headroom 991888719872B|'appdb_data_headroom'=96468992B;209715200;0 'master_data_headroom'=991888719872B;209715200;0 'model_data_headroom'=991888719872B;209715200;0 'msdb_data_headroom'=991888719872B;209715200;0 'tempdb_data_headroom'=991888719872B;209715200;0
```

**Flag hosts where headroom cannot be determined at all:**

```
check_mssql_databases "warning=data_headroom = -1" "critical=none"
OK: All 5 databases are ONLINE|'appdb_data_headroom'=96468992B;-1;0 'master_data_headroom'=991888719872B;-1;0 'model_data_headroom'=991888719872B;-1;0 'msdb_data_headroom'=991888719872B;-1;0 'tempdb_data_headroom'=991888719872B;-1;0
```
