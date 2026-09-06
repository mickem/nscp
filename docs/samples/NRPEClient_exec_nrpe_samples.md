**Send an execute request to a remote agent:**

An execute request invokes the remote agent's command-line interface and returns
its textual output — the equivalent of running `nscp <something>` there.

```
exec_nrpe host=192.168.56.103 command=help
Usage: nscp <command> [options]
...
```

**When the remote agent does not serve execute requests:**

Nothing comes back — no status, no message. NSClient++ exposes execute requests
only where its configuration allows them, and a stock Nagios `nrpe` daemon has
no such concept at all.

```
exec_nrpe host=127.0.0.1 port=15666 insecure=true command=help

```

**This is not how you run a check:**

An execute request returns raw text with no status to alert on. For a check, use
[`nrpe_query`](#nrpe_query):

```
nrpe_query host=127.0.0.1 port=15666 insecure=true command=check_drivesize
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
```

**Nothing listening:**

```
exec_nrpe host=127.0.0.1 port=15667 command=help
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15667 :Connection refused
```
