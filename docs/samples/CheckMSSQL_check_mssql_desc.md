#### About `check_mssql`

`check_mssql` verifies that a Microsoft SQL Server instance is **reachable and
answering queries**: it connects over ODBC, reads `SERVERPROPERTY(...)` and
`sys.dm_os_sys_info` and reports version, patch level, edition and uptime.
A reachable server is OK by default; a failed connection is UNKNOWN with the
stable message prefix `Failed to connect to SQL Server '<server>':` followed by
the ODBC diagnostic (SQLSTATE and native error included).

Keywords (one row per connected instance):

| Keyword         | Description                                                        |
|-----------------|--------------------------------------------------------------------|
| `server_name`   | Instance name (`SERVERPROPERTY('ServerName')`)                     |
| `version`       | Product version, e.g. `16.0.4265.3`                                |
| `product_level` | Patch level: `RTM`, `SPn` or `CUn`                                 |
| `edition`       | Edition, e.g. `Express Edition (64-bit)`                           |
| `uptime`        | Seconds since the server started (accepts units: `uptime < 1h`)    |

Defaults: no warning/critical expressions — being able to connect is the health
signal. Add thresholds when needed, e.g. alert after a restart
(`warning=uptime < 1h`) or pin the expected major version
(`critical=version not like '16.'`).

Connection options shared by all CheckMSSQL commands: `server` (host,
`host\INSTANCE` or `host,port`), `database`, `user`/`password` (leave both empty
for Windows integrated authentication as the NSClient++ service account),
`driver`, `connection-string` (raw override), `timeout` (login),
`query-timeout`, `trust-cert` and `encrypt`. Defaults come from the
`/settings/mssql` section, so credentials can be configured once in
`nsclient.ini` instead of per check.

The ODBC driver is auto-detected (newest installed *ODBC Driver NN for SQL
Server* first, falling back to the legacy `SQL Server` driver that ships with
Windows). On the modern drivers `TrustServerCertificate=yes` is added by
default since most instances run with a self-signed certificate; use
`trust-cert=false` (and optionally `encrypt=yes`) when the server has a
properly trusted certificate.
