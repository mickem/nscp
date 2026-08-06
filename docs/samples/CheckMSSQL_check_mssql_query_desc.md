#### About `check_mssql_query`

`check_mssql_query` runs a **user-supplied T-SQL statement** and turns every
returned column into a filter keyword, so warning/critical expressions and
perfdata can be built from any query result — the SQL Server counterpart of
`check_wmi`. Each returned row is matched against the filter separately.

Keywords (dynamic, one row per result-set row):

| Keyword    | Description                                                  |
|------------|--------------------------------------------------------------|
| `line`     | All columns of the row rendered as `column=value` pairs      |
| *(column)* | Every column of the result set, by name, usable as string or number |

Numeric columns can be thresholded directly (`warning=sessions > 50`) and are
emitted as perfdata when referenced. Alias columns in SQL (`SELECT COUNT(*) AS
sessions ...`) to give keywords stable, expression-friendly names — avoid
spaces and punctuation in column aliases.

Defaults: no warning/critical expressions and `empty-state=ignored`; set
`empty-state=ok` (plus `top-syntax=${status}: ${list}`) for queries where "no
rows" means healthy, as in the long-running-requests example.

The query runs with the connection's default database unless `database=` is
given; qualify object names (`msdb.dbo...`) or set `database=` when querying a
specific catalog. The statement runs under `query-timeout` (default 30s) so a
runaway query cannot hang the agent. The login used only needs SELECT/VIEW
SERVER STATE permissions appropriate to the query — prefer a low-privilege
monitoring login over `sa`.
