**Submit a passive result into a Mod-Gearman result queue:**

```
submit_gearman address=192.168.56.10:4730 key=<shared secret> command=cpu result=WARNING "message=cpu is busy|'load'=80%;70;90"
Submission successful
```

The core files it under the host name this agent reports (see `hostname` below)
and the service `cpu`, as a passive check.

**Submit a host result:**

The alias `host_check` is what makes the result a host result rather than a
service one:

```
submit_gearman address=192.168.56.10:4730 key=<shared secret> alias=host_check result=CRITICAL "message=host is down"
Submission successful
```

**Submit several results at once:**

`batch=` is repeatable and each value is a `command|result|message` record:

```
submit_gearman address=192.168.56.10:4730 key=<shared secret> "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
Submission successful
```

**The usual arrangement — route results rather than calling this by hand:**

```ini
[/settings/gearman/client]
channel = GEARMAN
; The name the core knows this host by; auto is the machine's own name.
hostname = auto

[/settings/gearman/client/targets/default]
address = 192.168.56.10:4730
key = <shared secret>
; check_results unless the core's module.conf names another result queue.
queue = check_results

[/settings/scheduler/schedules/default]
channel = GEARMAN
interval = 5m

[/settings/scheduler/schedules/cpu]
command = check_cpu "warning=load gt 80" "critical=load gt 90"
```

Every schedule then reports through the same gearmand the checks come from, and
the core files each one as a passive result for the service named after the
schedule. `check_and_forward` takes the same channel:

```
check_and_forward command=check_drivesize channel=GEARMAN alias=drivesize
Message submitted: GEARMAN
```

**When the key is missing:**

The key is the only thing separating a result this agent filed from one anybody
who can reach gearmand made up, so an encrypted submission without one is
refused rather than sent:

```
submit_gearman address=192.168.56.10:4730 command=cpu result=OK "message=all good"
No key for 192.168.56.10:4730. The key is the only thing separating a result this agent filed from one anybody who can reach gearmand made up, so an encrypted submission without one is refused. Set the target's key to the same value as the core's module.conf.
```

**Sending unencrypted has to be said out loud:**

`encryption=false` makes the payload plain base64, readable and forgeable by
anyone who can reach gearmand. It also needs the core's own
`accept_clear_results=yes`:

```
submit_gearman address=192.168.56.10:4730 encryption=false command=cpu result=OK "message=all good"
Encryption is off for 192.168.56.10:4730 but 'insecure' is not set. An unencrypted result is readable and forgeable by anyone who can reach gearmand; set insecure=true to say that is intended.
```

```
submit_gearman address=192.168.56.10:4730 encryption=false insecure=true command=cpu result=OK "message=all good"
Submission successful
```

**When gearmand is not there:**

The submission waits for gearmand's acknowledgement that the result is on the
queue, so an unreachable job server is reported rather than passing for success:

```
submit_gearman address=192.168.56.11:4730 key=<shared secret> command=cpu result=OK "message=all good"
Gearman error: Failed to connect to 192.168.56.11:4730: Connection refused
```

**When results simply never appear on the core:**

Nothing fails in that case — gearmand accepts a result for a queue nobody reads,
and the core ignores one whose `host_name` it does not know. Check, in order:
the `key` against the core's `module.conf`, the `queue` against its
`result_queue`, and `hostname` against the `host_name` in the core's object
configuration.
