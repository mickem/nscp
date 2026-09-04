**Submit a passive result to an NRDP endpoint:**

```
submit_nrdp target=nrdp command=nightly_backup result=CRITICAL "message=backup failed"
OK: Message submitted
```

**A typical target:**

Keep the token in the settings rather than on the command line — it is the only
credential NRDP has, and a command line ends up in process listings and logs.

```ini
[/settings/NRDP/client/targets/nrdp]
address = https://nagios.example.com/nrdp/
token = <security token>
verify mode = peer
tls version = 1.3
ca = /etc/ssl/certs/ca-certificates.crt
```

**Submit several results at once:**

Results that arrive together are batched into one HTTP round trip rather than
one request per check.

```
submit_nrdp target=nrdp "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
OK: Message submitted
```

**Route results rather than calling this by hand:**

```ini
[/settings/scheduler/schedules/disk]
command = check_drivesize
interval = 5m
channel = NRDP
```

**Through a corporate proxy:**

```ini
[/settings/NRDP/client/targets/nrdp]
proxy = http://proxy.example.com:3128
no proxy = localhost,127.0.0.1,.internal.example.com
```

**Nothing listening:**

```
submit_nrdp host=127.0.0.1 port=15670 command=nightly_backup result=CRITICAL "message=backup failed" token=secret
UNKNOWN: Error: Failed to connect to 127.0.0.1:15670: Connection refused
```

**Use HTTPS:**

The token is sent with every submission, so plain HTTP puts it on the wire in
the clear. `verify mode` defaults to `peer` and `tls version` to 1.3; point `ca`
at the bundle that signs the endpoint's certificate rather than turning
verification off.
