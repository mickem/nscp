**Check that a MySQL/MariaDB server is up (connecting is the health signal):**

```
check_mysql host=127.0.0.1 user=monitor password=secret
OK: mariadb 11.8.8-MariaDB-ubu2404, uptime 771002s, connections 3/151 (1%)
```

**Warn when the server restarted recently (uptime keyword supports time units):**

```
check_mysql host=127.0.0.1 user=monitor password=secret "warning=uptime < 15m"
WARNING: mariadb 11.8.8-MariaDB-ubu2404, uptime 2s, connections 1/151 (0%)|'mariadb_uptime'=2s;900;0
```

**Alert when the connection pool is close to max_connections:**

```
check_mysql host=127.0.0.1 user=monitor password=secret "warning=connections_pct > 60" "critical=connections_pct > 80"
OK: mariadb 11.8.8-MariaDB-ubu2404, uptime 771002s, connections 3/151 (1%)|'mariadb_connections_pct'=1%;60;80
```

**An unreachable or refusing server is clearly reported (UNKNOWN):**

```
check_mysql host=127.0.0.1 port=9999 timeout=2
Failed to connect to MySQL server '127.0.0.1:9999': Can't connect to server on '127.0.0.1' (110)
```

```
check_mysql host=127.0.0.1 user=root password=wrong
Failed to connect to MySQL server '127.0.0.1:3306': Access denied for user 'root'@'192.168.127.1' (using password: YES)
```

**Connect through the local socket instead of TCP:**

```
check_mysql socket=/run/mysqld/mysqld.sock user=monitor password=secret
OK: mariadb 11.8.8-MariaDB-ubu2404, uptime 771002s, connections 3/151 (1%)
```

**MySQL 8 with a non-default client-plugin directory (caching_sha2_password):**

```
check_mysql host=db1 user=monitor password=secret plugin-dir=C:\Program Files\MariaDB\MariaDB Connector C 64-bit\lib\plugin
OK: mysql 8.4.11, uptime 1011s, connections 1/151 (0%)
```
