**Report the running agent version:**

```
check_version
OK: 0.18.1 2026-08-14
```

**As a heartbeat check over NRPE:**

The command never fails on its own, so anything other than OK means the agent
is not answering — which makes it the cheapest possible "is this agent alive"
service.

```
check_nrpe --host 192.168.56.103 --command check_version
OK: 0.18.1 2026-08-14
```

**When you want to alert on the version rather than just report it:**

`check_version` returns a string with no thresholds. Use CheckNSCP's
[`check_nscp_version`](CheckNSCP.md#check_nscp_version) instead, which exposes
the parts as numbers:

```
check_nscp_version "crit=major < 12"
OK: 0.18.1 (2026-08-14)
```
