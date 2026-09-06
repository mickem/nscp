**Submit a passive result to an Icinga 2 server:**

```
submit_icinga target=icinga command=nightly_backup result=CRITICAL "message=backup failed"
OK: Message submitted
```

**A typical target:**

The Icinga 2 API listens on 5665 and is HTTPS-only. Its certificate is normally
issued by the Icinga CA rather than a public one, so point `ca` at that CA
instead of disabling verification.

```ini
[/settings/icinga/client/targets/icinga]
address = https://icinga.example.com:5665
username = nscp
password = <api password>
ca = /var/lib/icinga2/ca/ca.crt
tls version = 1.3
check source = web01
check command = passive
```

**Route results rather than calling this by hand:**

```ini
[/settings/scheduler/schedules/disk]
command = check_drivesize
interval = 5m
channel = ICINGA
```

**Submit several results at once:**

```
submit_icinga target=icinga "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
OK: Message submitted
```

**When the host or service object does not exist:**

Icinga addresses results by object, so a submission for an object it does not
know is rejected by the server:

```
submit_icinga target=icinga command=nightly_backup result=CRITICAL "message=backup failed"
UNKNOWN: Icinga error: No objects found.
```

Either create the objects in the Icinga configuration, or let the module create
them:

```ini
[/settings/icinga/client/targets/icinga]
ensure objects = true
host template = generic-host
service template = generic-service
```

Enable that deliberately — it lets an agent create objects in your monitoring
configuration, so give the API user only the permissions it needs.

**Nothing listening:**

```
submit_icinga host=127.0.0.1 port=15670 command=nightly_backup result=CRITICAL "message=backup failed" username=root password=secret
UNKNOWN: Network error: Failed to connect to 127.0.0.1:15670: Connection refused
```
