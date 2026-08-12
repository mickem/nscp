**Default check on an instance that never ran CHECKDB — warns:**

```
check_mssql_integrity
WARNING: 3/3 databases (master: checkdb age -1s, 0 suspect pages, model: checkdb age -1s, 0 suspect pages, msdb: checkdb age -1s, 0 suspect pages)|'master_checkdb_age'=-1s;1209600;0 'master_suspect_pages'=0;0;0 'model_checkdb_age'=-1s;1209600;0 'model_suspect_pages'=0;0;0 'msdb_checkdb_age'=-1s;1209600;0 'msdb_suspect_pages'=0;0;0
```

**After `DBCC CHECKDB` has run — ages track the last successful check:**

```
check_mssql_integrity
OK: All 3 databases checked recently, no suspect pages|'master_checkdb_age'=5s;1209600;0 'master_suspect_pages'=0;0;0 'model_checkdb_age'=2s;1209600;0 'model_suspect_pages'=0;0;0 'msdb_checkdb_age'=1s;1209600;0 'msdb_suspect_pages'=0;0;0
```

**Tighter age for a nightly CHECKDB job (time units):**

```
check_mssql_integrity "warning=checkdb_age > 7d" "critical=suspect_pages > 0"
OK: All 3 databases checked recently, no suspect pages|'master_checkdb_age'=5s;604800;0 'master_suspect_pages'=0;0;0 'model_checkdb_age'=2s;604800;0 'model_suspect_pages'=0;0;0 'msdb_checkdb_age'=1s;604800;0 'msdb_suspect_pages'=0;0;0
```
