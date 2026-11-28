---
icon: "🔧"
modules: [CheckMSSQL, CheckMySQL, CheckWMI, CheckDocker, CheckKubernetes]
action: none
---
**Web UI: pages for the database, WMI, Docker and Kubernetes checks.**
Nothing to do.

- **Microsoft SQL Server**: the server and database, or an ODBC connection
  string (which then replaces every other field, and is flagged when it holds
  a password); Windows authentication as the agent's account, or a SQL login;
  the driver; the fact sets.
- **MySQL / MariaDB**: over TCP or through a local socket (which wins over
  host and port), *Require TLS* - flagged when off, since TLS is then left to
  the client library - and the login set here or read from an option file.
- **WMI**: what callers may query - anything, plain SELECTs from the classes
  and namespaces listed, or only the named queries - with the named queries
  and the remote hosts (and their logins) below.
- **Docker**: the engine's endpoint, which has to be a local socket or named
  pipe, and the fact sets.
- **Kubernetes**: the cluster through the pod's service account, a kubeconfig
  file, or an API server and token, in the agent's order of precedence; a
  token sent over `http://`, an unverified API server and a token stored in
  the settings are flagged.

Each of these pages has a *Test connection* button that runs the module's own
read-only health check (`check_mssql`, `check_mysql`, `check_docker_info`,
`check_kubernetes`, or a small WMI query) against the settings the agent has
loaded, and shows what it answered; it is off while there are unsaved changes.

Both database pages say plainly that every check takes the server (and, for
MSSQL, the connection string) from its caller, and sends the configured login
there: give those checks only to callers you trust.
