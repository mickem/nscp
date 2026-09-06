**Submit a passive result to a remote agent:**

```
submit_nrpe host=192.168.56.103 command=nightly_backup result=CRITICAL "message=backup failed"
OK: Message submitted
```

**Submit several results over one connection:**

`batch=` is repeatable and each value is a `command|result|message` record.

```
submit_nrpe host=192.168.56.103 "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
OK: Message submitted
```

**When the receiving end does not accept submissions:**

A stock Nagios `nrpe` daemon has no notion of passive results, and an
NSClient++ agent only accepts them where its configuration allows. The far end
answers as if you had asked it to *run* the named command:

```
submit_nrpe host=127.0.0.1 port=15666 insecure=true command=nightly_backup result=CRITICAL "message=backup failed"
UNKNOWN: Unknown command(s): nightly_backup
```

This is the usual reason to reach for a transport designed for passive results
instead — [NSCA-ng](NSCANgClient.md), [NRDP](NRDPClient.md) or
[NSCA](NSCAClient.md) — or, if the far end is NSClient++,
[`submit_remote_nscp`](NSCPClient.md#submit_remote_nscp), which has no payload
ceiling and carries performance data as structured data.

**Nothing listening:**

```
submit_nrpe host=127.0.0.1 port=15667 command=nightly_backup result=OK "message=done"
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15667 :Connection refused
```
