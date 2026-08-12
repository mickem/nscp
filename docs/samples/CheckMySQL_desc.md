CheckMySQL checks MySQL, MariaDB, Percona and other MySQL-compatible database
servers on both Windows and Linux. It connects with MariaDB Connector/C
(speaking the native protocol, so one module covers the whole MySQL family)
and is only built/shipped when the connector is available.

Default connection settings live under `/settings/mysql` (host, port, socket,
user, password, TLS, timeouts) and every command accepts the same overrides
(`host=`, `port=`, `user=`, `password=`, `socket=`, `tls=true`, ...).
Credentials can also be kept out of `nsclient.ini` entirely via
`defaults file` pointing at a permission-protected my.cnf-style file.
