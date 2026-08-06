**Default check (full backup required within 7 days, warn after 3):**

```
check_mssql_backup
OK: All 3 databases have recent backups|'model_full_age'=66s;259200;0 'master_full_age'=66s;259200;0 'msdb_full_age'=66s;259200;0
```

**A database that has never been backed up (age is -1):**

```
check_mssql_backup
CRITICAL: 1/4 databases (appdb: last full backup -1s ago)|'appdb_full_age'=-1s;259200;0 'model_full_age'=87s;259200;0 'master_full_age'=87s;259200;0 'msdb_full_age'=87s;259200;0
```

**Log-backup age for FULL-recovery databases:**

```
check_mssql_backup "filter=recovery_model = 'FULL'" "warning=none" "critical=log_age < 0 or log_age > 1h" "detail-syntax=${name}: last log backup ${log_age}s ago"
CRITICAL: 1/1 databases (model: last log backup -1s ago)|'model_log_age'=-1s;0;0
```

**Custom thresholds with age units:**

```
check_mssql_backup "warning=full_age > 25h" "critical=full_age < 0 or full_age > 2d"
OK: All 4 databases have recent backups|'appdb_full_age'=5s;90000;0 'model_full_age'=248s;90000;0 'master_full_age'=248s;90000;0 'msdb_full_age'=248s;90000;0
```

**Exclude databases that are not backed up on purpose:**

```
check_mssql_backup "filter=name != 'model' and name != 'appdb'"
OK: All 2 databases have recent backups|'master_full_age'=248s;259200;0 'msdb_full_age'=248s;259200;0
```
