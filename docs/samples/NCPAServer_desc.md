`NCPAServer` speaks the [Nagios Cross-Platform Agent](https://www.nagios.org/ncpa/) HTTP API, so Nagios Core and
Nagios XI can poll this agent with the stock `check_ncpa.py` plugin and the XI NCPA wizard, unmodified. It listens on
HTTPS port `5693` - the port and the protocol `check_ncpa` defaults to - and answers `/api/<node path>` with either a
subtree of the node tree or a finished check result.

The module measures nothing itself. Everything under `cpu`, `memory`, `disk` and `interface` is rendered from the 1 Hz
metrics snapshot the agent's collectors already publish, `services` and `processes` are answered by dispatching
`check_service` and `check_process`, and `plugins/` hands the request straight to the agent's own query dispatch - which
is what puts **every** NSClient++ check behind `check_ncpa` with no mapping work.

To use this module you need to enable it and set a token:

```
[/modules]
NCPAServer = enabled

[/settings/NCPA/server]
token = a-long-random-shared-secret
allowed hosts = 192.168.0.10
```

#### Securing the server

*   **`token`** - required. The shared secret every request carries as `?token=...`, which `check_ncpa.py` sends with
    `-t`. There is no default: until one is set the server answers every request with NCPA's
    `{"error": "Incorrect credentials given."}`, so an agent that boots with this module enabled but unconfigured
    exposes nothing. The comparison is constant time, and a rejected token is never echoed into the log.
*   **`backup token`** - a second accepted token. Set it to the new secret while the monitoring servers are being moved
    over, then swap it into `token` and clear this - which is how the token is rotated without a window where checks
    fail.
*   **`allowed hosts`** - restrict which addresses may connect at all, inherited from `[/settings/default]` unless set
    here. A refused address gets HTTP 403 rather than an NCPA error body, so it is reported as a connection failure by
    the plugin instead of as a CRITICAL check result.
*   **`allow insecure`** - the server refuses to start without a TLS certificate rather than putting the token on the
    wire in clear on every check. The default certificate path is the web server's, so one certificate serves both.
    `check_ncpa.py` does not verify it unless it is called with `-s`, so a self-signed certificate works out of the box;
    pass `-s` (and a certificate the monitoring server trusts) if you want the connection authenticated as well as
    encrypted.
*   Repeated bad tokens from one address are rate limited (`auth rate limit max failures`,
    `auth rate limit block seconds`), so the token cannot be guessed at machine speed.

#### The `plugins` node and `allow arguments`

`plugins/<name>` runs a registered query, an alias or an external script and returns its output unchanged:

```
check_ncpa.py -H 192.168.0.5 -t <token> -M plugins/check_uptime
```

Two settings cap it:

| Setting           | Default | Effect                                                                                       |
|-------------------|---------|----------------------------------------------------------------------------------------------|
| `plugins`         | `any`   | `any` exposes every registered query by name; `scripts` exposes only the commands an external-scripts module registered (asked of the core, not guessed from the name); anything else is read as a comma separated list of query names and nothing outside it is callable. |
| `allow arguments` | `false` | Whether a caller may put arguments on the path. Off by default, the same default and the same reasoning as the NRPE server's option of this name: a caller that can shape a check's arguments can ask the agent rather more than the operator meant to offer. |

With `allow arguments = true`, each `-a` token becomes one path segment and reaches the query as one REST-style
`key=value` token:

```
check_ncpa.py -H 192.168.0.5 -t <token> -M plugins/check_cpu -a "warning=load>80 critical=load>90"
```

A query outside the allow-list, and an argument sent while `allow arguments` is off, are both answered as UNKNOWN with
a message saying so rather than being silently ignored.

#### Differences from the Nagios NCPA agent

The wire format, the JSON shapes and the Nagios output text are reproduced faithfully - including NCPA's own quirks,
such as a bare threshold alerting on any negative value and a percentage-primary node dropping its own perfdata. Where
the underlying data differs, it differs like this:

| Node                                | Here                                                                                                          |
|-------------------------------------|---------------------------------------------------------------------------------------------------------------|
| `cpu/{user,system,idle}`            | A share of the last five minutes with a `%` unit, not cumulative milliseconds. `-w 80` therefore means "80 percent", which is the threshold the switch reads as anyway. |
| `interface/*`, `disk/physical/*`    | Already per-second rates, because that is what the collector publishes. `-d` (delta) appends `/s` to the unit and leaves the number alone rather than differencing two rates. |
| `disk/mount`                        | Not present: the agent enumerates mounted volumes only, so everything is under `disk/logical`.                  |
| `processes`                         | Filterable by `name`, `exe`, `username`, `cmd`, `match` and `combiner`. `cpu_percent` and `mem_percent` are not available per process, so those two filters are not supported. |
| Windows event log (`logs`)          | Not present. `check_eventlog` is reachable through `plugins/` with its full filter language.                    |

`delta` samples are kept in memory rather than in temp files, so the first poll after a restart reports zero for a
delta check - one interval, once, rather than a persistent state file to manage.
