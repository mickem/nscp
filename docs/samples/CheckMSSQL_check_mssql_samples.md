**Default check (local default instance, Windows authentication):**

```
check_mssql
OK: DBSRV01: SQL Server 16.0.4265.3 RTM Developer Edition (64-bit), uptime 144s
```

**Warn when the server restarted recently (age units supported):**

```
check_mssql "warning=uptime < 1h"
WARNING: DBSRV01: SQL Server 16.0.4265.3 RTM Developer Edition (64-bit), uptime 144s|'DBSRV01_uptime'=144s;3600;0
```

**Custom output listing edition and patch level:**

```
check_mssql "top-syntax=%(status): %(list)" "detail-syntax=%(server_name) is running %(edition) (%(version) %(product_level))"
OK: DBSRV01 is running Developer Edition (64-bit) (16.0.4265.3 RTM)
```

**Against a named instance or remote host with SQL authentication:**

```
check_mssql "server=db1.example.com,1433" user=monitor password=...
OK: DBSRV01: SQL Server 16.0.4265.3 RTM Developer Edition (64-bit), uptime 144s
```

**When the server is unreachable (stable UNKNOWN contract):**

```
check_mssql
UNKNOWN: Failed to connect to SQL Server 'localhost': [08001/17] [Microsoft][ODBC SQL Server Driver][DBNETLIB]SQL Server does not exist or access denied., [01000/2] [Microsoft][ODBC SQL Server Driver][DBNETLIB]ConnectionOpen (Connect()).
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_mssql --argument "warning=uptime < 10m"
OK: DBSRV01: SQL Server 16.0.4265.3 RTM Developer Edition (64-bit), uptime 144s
```
