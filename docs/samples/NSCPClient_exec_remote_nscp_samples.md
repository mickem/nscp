**Send an execute request to a remote agent:**

An execute request invokes the remote agent's command-line interface and returns
its textual output — the equivalent of running `nscp <something>` on that host.

```
exec_remote_nscp target=web01 command=help
Usage: nscp <command> [options]
...
```

**Inspect a remote agent's settings:**

```
exec_remote_nscp target=web01 command=settings "argument=--list" "argument=--path" "argument=/modules"
CheckDisk = enabled
CheckHelpers = enabled
CheckSystem = enabled
NRPEServer = enabled
```

**This is not how you run a check:**

An execute request returns raw text with no status to alert on. For a check, use
[`remote_nscp_query`](#remote_nscp_query):

```
remote_nscp_query target=web01 command=check_drivesize
WARNING: WARNING C:\: 91.2GB/100GB used
```

**Nothing listening:**

```
exec_remote_nscp host=127.0.0.1 port=15669 command=help
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15669 :Connection refused
```

Because an execute request is closer to remote administration than to
monitoring, be deliberate about which agents accept it and from where; the
remote agent's own configuration decides whether it serves them at all.
