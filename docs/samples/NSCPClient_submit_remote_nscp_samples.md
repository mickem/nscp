**Submit a passive result to a remote agent:**

```
submit_remote_nscp target=relay command=nightly_backup result=CRITICAL "message=backup failed"
OK: Message submitted
```

**Submit several results over one connection:**

`batch=` is repeatable and each value is a `command|result|message` record.

```
submit_remote_nscp target=relay "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
OK: Message submitted
```

**The usual arrangement — route results rather than calling this by hand:**

A host that cannot reach the monitoring server submits to one that can:

```ini
[/modules]
NSCPClient = enabled
Scheduler = enabled

[/settings/NSCP/client/targets/relay]
address = nscp://10.0.2.10:8443
password = <shared secret>
verify mode = peer
ca = /etc/nsclient/ca.pem

[/settings/scheduler/schedules/disk]
command = check_drivesize
interval = 5m
channel = NSCP
```

Every five minutes the check runs locally and its result is submitted to the
relay, which forwards it onward.

**Nothing listening:**

```
submit_remote_nscp host=127.0.0.1 port=15669 command=nightly_backup result=OK "message=done"
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15669 :Connection refused
```

**Why this rather than `submit_nrpe`:**

There is no payload ceiling here and performance data travels as structured data
rather than being flattened into the message, so a full check result survives
the hop intact.
