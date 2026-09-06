**Return CRITICAL with the default message:**

```
check_critical
CRITICAL: No message
```

**Return CRITICAL with your own message:**

```
check_critical "message=Service is down"
CRITICAL: Service is down
```

**As a placeholder for a check that is not implemented yet:**

Wiring a service to `check_critical` makes the gap visible on the dashboard
instead of leaving a silently missing check.

```
check_nrpe --host 192.168.56.103 --command check_critical --arguments "message=TODO: implement backup verification"
CRITICAL: TODO: implement backup verification
```
