**Default check on an instance that never ran CHECKDB — warns:**

```
check_mssql_integrity
WARNING: 4/4 databases (appdb: checkdb age -1s, 0 suspect pages, master: checkdb age -1s, 0 suspect pages, model: checkdb age -1s, 0 suspect pages, msdb: checkdb age -1s, 0 suspect pages)|'appdb_checkdb_age'=-1s;1209600;0 'appdb_suspect_pages'=0;0;0 'master_checkdb_age'=-1s;1209600;0 'master_suspect_pages'=0;0;0 'model_checkdb_age'=-1s;1209600;0 'model_suspect_pages'=0;0;0 'msdb_checkdb_age'=-1s;1209600;0 'msdb_suspect_pages'=0;0;0
```

**After `DBCC CHECKDB` has run — ages track the last successful check:**

```
check_mssql_integrity
OK: No integrity problems found in 4 databases|'appdb_checkdb_age'=229s;1209600;0 'appdb_suspect_pages'=0;0;0 'master_checkdb_age'=230s;1209600;0 'master_suspect_pages'=0;0;0 'model_checkdb_age'=230s;1209600;0 'model_suspect_pages'=0;0;0 'msdb_checkdb_age'=230s;1209600;0 'msdb_suspect_pages'=0;0;0
```

**Tighter age for a nightly CHECKDB job (time units):**

```
check_mssql_integrity "warning=checkdb_age > 7d" "critical=suspect_pages > 0"
OK: No integrity problems found in 4 databases|'appdb_checkdb_age'=229s;604800;0 'appdb_suspect_pages'=0;0;0 'master_checkdb_age'=230s;604800;0 'master_suspect_pages'=0;0;0 'model_checkdb_age'=230s;604800;0 'model_suspect_pages'=0;0;0 'msdb_checkdb_age'=230s;604800;0 'msdb_suspect_pages'=0;0;0
```

**A monitoring login without sysadmin or msdb access — reports what it cannot
determine instead of failing the check:**

```
check_mssql_integrity "warning=none" "critical=none" "top-syntax=${status}: ${list}" "detail-syntax=${name}: age ${checkdb_age} pages ${suspect_pages}" show-all
OK: appdb: age -2 pages -1, master: age -1 pages -1, model: age -2 pages -1, msdb: age -1 pages -1
```

`checkdb_age` is `-2` for the databases whose boot page the login may not read
(`DBCC DBINFO` needs sysadmin) and `suspect_pages` is `-1` when
`msdb.dbo.suspect_pages` is out of reach. Neither sentinel trips the default
thresholds, so a least-privilege login degrades quietly rather than alerting or
turning the whole check UNKNOWN.
