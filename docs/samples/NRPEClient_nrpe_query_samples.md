`nrpe_query` is an alias of [`check_nrpe`](#check_nrpe) — same implementation,
same options, same behaviour.

**Run a check on the remote host:**

```
nrpe_query host=127.0.0.1 port=15666 insecure=true command=check_ok "argument=message=hello from NRPE"
OK: hello from NRPE
```

**With a configured target:**

```
nrpe_query target=web01 command=check_drivesize
WARNING: WARNING C:\: 91.2GB/100GB used
```

See [`check_nrpe`](#check_nrpe) for the full set of examples — targets, batching,
protocol versions, payload length and the TLS failure modes.
