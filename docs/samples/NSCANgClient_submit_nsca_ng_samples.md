**Submit a passive result to an NSCA-ng server:**

```
submit_nsca_ng target=nsca-ng command=nightly_backup result=CRITICAL "message=backup failed"
OK: Message submitted
```

**A typical target:**

```ini
[/settings/NSCA-NG/client/targets/nsca-ng]
address = nsca-ng://192.168.56.10:5668
identity = web01
password = <shared secret>
ca = /etc/nsclient/ca.pem
max output length = 65536
```

`identity` and `password` must match a client entry in the server's
`nsca-ng.cfg`. The identity is an authorisation boundary, not just a label — it
is what the server uses to decide which hosts and services this client may
submit results for.

**Submit a host check rather than a service check:**

The command-line option is `host-check` and it is a **bare flag** — it takes no
value, so `host-check=true` is rejected with an "Invalid command line" error:

```
submit_nsca_ng target=nsca-ng host-check result=OK "message=host is up"
OK: Message submitted
```

Because it is a flag rather than a valued option, it also **cannot be set over
REST**, which passes every argument as a `key=value` token. Set it on the target
instead, where the settings key is `host check` (the legacy alias `host_check`
is still honoured):

```ini
[/settings/NSCA-NG/client/targets/nsca-ng]
host check = true
```

**Submit several results at once:**

```
submit_nsca_ng target=nsca-ng "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
OK: Message submitted
```

**Route results rather than calling this by hand:**

```ini
[/settings/scheduler/schedules/disk]
command = check_drivesize
interval = 5m
channel = NSCA-NG
```

**Nothing listening:**

```
submit_nsca_ng host=127.0.0.1 port=15670 command=nightly_backup result=CRITICAL "message=backup failed" identity=agent1 password=secret
UNKNOWN: NSCA-NG network error: connect to 127.0.0.1:15670 failed: Connection refused
```

**Long output survives:**

`max output length` defaults to 65536 bytes, against NSCA's 512-byte payload, so
a full check message arrives intact — as long as it is also within the server's
own limit, which truncates on its side.

**`insecure` removes the point of using NSCA-ng:**

It disables peer verification, so you lose the guarantee that you are talking to
your own server. Use it only while bringing a deployment up, and point `ca` at
the server's CA instead.

Like `host-check` it is a bare flag on the command line — `insecure=true` is
rejected, and it cannot be set over REST at all:

```
submit_nsca_ng target=nsca-ng insecure command=nightly_backup result=OK "message=done"
OK: Message submitted
```

```ini
[/settings/NSCA-NG/client/targets/nsca-ng]
insecure = true
```

**Custom relay commands:**

Handlers defined under `[/settings/NSCA-NG/client/handlers]` are registered
automatically as `submit_<alias>` commands, so a relay with several destinations
does not need a module instance per destination.
