`remote_nscp_query` is an alias of
[`check_remote_nscp`](#check_remote_nscp) — same implementation, same options,
same behaviour.

**Run a check on a remote agent:**

```
remote_nscp_query target=web01 command=check_drivesize
WARNING: WARNING C:\: 91.2GB/100GB used
```

**Nothing listening:**

```
remote_nscp_query host=127.0.0.1 port=15669 command=check_ok
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15669 :Connection refused
```

See [`check_remote_nscp`](#check_remote_nscp) for the full set of examples —
targets, arguments, the password and TLS options, and why NSCP is preferable to
NRPE when both ends are NSClient++.
