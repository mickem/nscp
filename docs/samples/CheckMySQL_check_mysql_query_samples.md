**List rows returned by a custom query (each column becomes a keyword):**

```
check_mysql_query host=127.0.0.1 user=monitor password=secret "query=SELECT table_schema AS db, COUNT(*) AS tables_count FROM information_schema.tables GROUP BY table_schema" "detail-syntax=%(db)=%(tables_count)" "top-syntax=${list}"
information_schema=85, mysql=31, performance_schema=81, sys=102
```

**Threshold on a column value (e.g. long-running queries):**

```
check_mysql_query host=127.0.0.1 user=monitor password=secret "query=SELECT COUNT(*) AS slow_queries FROM information_schema.processlist WHERE time > 60 AND command != 'Sleep'" "critical=slow_queries > 0" "top-syntax=${status}: ${list}" "detail-syntax=%(slow_queries) slow queries"
OK: 0 slow queries
```

**Emit the thresholded column as performance data (set a perf-syntax):**

```
check_mysql_query host=127.0.0.1 user=monitor password=secret "query=SELECT COUNT(*) AS tables_count FROM information_schema.tables" "critical=tables_count > 5000" "perf-syntax=tables" "top-syntax=${status}: ${list}" "detail-syntax=%(tables_count) tables"
OK: 299 tables|'tables_counttables'=299;0;5000
```

**A statement that returns no result set is reported instead of a silent OK:**

```
check_mysql_query host=127.0.0.1 user=monitor password=secret "query=SET @x = 1"
Query returned no result set (the statement produced no columns)
```

**A missing query is rejected with a clear message:**

```
check_mysql_query host=127.0.0.1 user=monitor password=secret
No query specified (use query=<SQL>)
```
