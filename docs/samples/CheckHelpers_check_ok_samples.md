**Return OK with the default message:**

```
check_ok
OK: No message
```

**Return OK with your own message:**

```
check_ok "message=Database backup completed"
OK: Database backup completed
```

**As a connectivity probe over NRPE:**

Nothing on the far end can make this fail, so a non-OK answer means the
transport, the permissions or the agent itself is the problem — not the host
being checked.

```
check_nrpe --host 192.168.56.103 --command check_ok
OK: No message
```
