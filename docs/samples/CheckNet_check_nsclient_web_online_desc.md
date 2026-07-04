#### About `check_nsclient_web_online`

`check_nsclient_web_online` queries the REST API of a **remote** NSClient++ 
agent over HTTPS. It has two modes:

* **Reachability probe** (no `command=`): it hits `/api/v1/info` and reports
  **OK** `REST API reachable …` when the agent answers, **CRITICAL** when it
  cannot be reached or authentication fails.
* **Remote check** (`command=<check>`): it runs that check on the remote agent
  (`/api/v1/queries/<check>/commands/execute`) and passes the remote Nagios
  status **and** message straight through, so the local result mirrors what the
  remote agent returned.

This is intended for *liveness / availability* monitoring of an agent from a
central host. (A fuller "run remote checks" command — `check_nsclient_web` — is
planned separately; this one focuses on whether the web API is online.)

Arguments:

| Argument            | Description                                                        |
|---------------------|-------------------------------------------------------------------|
| `url`               | Base URL of the remote agent, e.g. `https://host:8443`            |
| `host` / `port`     | Alternative to `url`; `port` defaults to `8443`                   |
| `password`          | REST API password (sent as the `password` header, as user `admin`)|
| `user`              | Optional username → switches to HTTP Basic authentication         |
| `command`           | Remote check to run (omit for a plain reachability probe)         |
| `argument`          | Argument for the remote check; repeat for multiple                |
| `timeout`           | Request timeout in milliseconds                                   |
| `tls-version`       | TLS version (default `tlsv1.2+`)                                  |
| `verify`            | Certificate verify mode (default `none`, for self-signed agents)  |
| `ca`                | CA bundle to verify the remote certificate                        |

By default the remote certificate is **not** verified (`verify=none`) because
agents commonly present a self-signed certificate; set `verify=peer` with `ca=`
to enforce verification.
