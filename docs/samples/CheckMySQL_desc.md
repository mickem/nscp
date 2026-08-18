CheckMySQL checks MySQL, MariaDB, Percona and other MySQL-compatible database
servers on both Windows and Linux. It connects with MariaDB Connector/C
(speaking the native protocol, so one module covers the whole MySQL family)
and is only built/shipped when the connector is available.

Default connection settings live under `/settings/mysql` (host, port, socket,
user, password, TLS, timeouts) and every command accepts the network overrides
per request (`host=`, `port=`, `user=`, `password=`, `tls=true`, ...). The
parameters that name a local resource the service loads or reads on its own
account — `socket`, `plugin dir` and `defaults file` — are configured only in
`/settings/mysql`, never per request, so a check caller cannot make the agent
load a plugin or read a file it chooses. Credentials can be kept out of
`nsclient.ini` entirely via `defaults file` pointing at a permission-protected
my.cnf-style file.
