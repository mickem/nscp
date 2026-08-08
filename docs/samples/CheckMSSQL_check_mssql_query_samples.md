**List rows returned by a query (each column becomes a keyword):**

```
check_mssql_query "query=SELECT name, database_id FROM sys.databases"
name=master, database_id=1, name=tempdb, database_id=2, name=model, database_id=3, name=msdb, database_id=4
```

**Threshold on a computed value (user sessions):**

```
check_mssql_query "query=SELECT COUNT(*) AS sessions FROM sys.dm_exec_sessions WHERE is_user_process = 1" "warning=sessions > 50" "critical=sessions > 100" "top-syntax=${status}: ${list}"
OK: sessions=4
```

**Alert on rows matching a condition (long-running requests):**

```
check_mssql_query "query=SELECT session_id, total_elapsed_time FROM sys.dm_exec_requests WHERE total_elapsed_time > 60000" "critical=total_elapsed_time > 60000" "empty-state=ok" "top-syntax=${status}: ${list}" "detail-syntax=session ${session_id}: ${total_elapsed_time}ms" "empty-syntax=%(status): no long-running requests"
OK: no long-running requests
```

**A batch whose first statement returns a row count instead of rows:**

```
check_mssql_query "query=CREATE TABLE #t(i int); INSERT INTO #t VALUES(1),(2); SELECT COUNT(*) AS n FROM #t;" "top-syntax=${status}: ${list}"
OK: n=2
```

**Missing query (stable error contract):**

```
check_mssql_query
UNKNOWN: No query specified (use query=<T-SQL>)
```

**A statement that returns no result set at all:**

```
check_mssql_query "query=DECLARE @i int = 1;"
UNKNOWN: Query returned no result set (the statement produced no columns)
```
