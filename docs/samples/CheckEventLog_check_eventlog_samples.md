**Default check (warnings and errors in the last 24 hours):**

The default filter is `level in ('warning', 'error', 'critical')`, critical is
`level in ('error', 'critical')` and the scan range is `-24h`.

```
check_eventlog
CRITICAL: 3 message(s) Application Application Error (Faulting application name: svc.exe, version 1.0.0.0), Application MyApp (Failed to open database), System Service Control Manager (The MyService service terminated unexpectedly.)
```

A quiet log:

```
check_eventlog
OK: Event log seems fine
```

**Pick the logs to scan (`file=` / `log=`, repeatable):**

Note that several `file=` values produce one aggregate set, not one result per
log.

```
check_eventlog file=System file=Application "filter=level = 'error'"
CRITICAL: 1 message(s) System Service Control Manager (The MyService service terminated unexpectedly.)
```

**Narrow the time window:**

```
check_eventlog file=Application scan-range=-1h "filter=level = 'error'"
OK: Event log seems fine
```

**Alert on a specific event id:**

```
check_eventlog file=Application "filter=id = 1000" "warning=count > 0" "detail-syntax=${id}: ${message}"
WARNING: 1 message(s) 1000: Faulting application name: svc.exe, version 1.0.0.0
```

**Scope to one provider and count occurrences:**

```
check_eventlog file=Security "filter=id = 4625" "warning=count > 5" "crit=count > 20" "top-syntax=${status}: ${count} failed logons"
WARNING: 9 failed logons
```

**Filter by the account SID:**

```
check_eventlog file=Security "filter=user = 'S-1-5-18'" "detail-syntax=${id}: ${message}"
OK: Event log seems fine
```

**Collapse repeats with `unique`:**

`unique` is shorthand for `unique-index=${log}-${source}-${id}`, so a message
repeated fifty times is reported once.

```
check_eventlog file=Application "filter=level = 'error'" unique "detail-syntax=${id}"
CRITICAL: 1 message(s) 1000
```

**Only report events seen since the last run (`bookmark`):**

The first run reports everything in the scan range; subsequent runs with the
same bookmark name report only what is new, and the position survives a restart.

```
check_eventlog file=Application bookmark=auto "filter=level = 'error'"
CRITICAL: 2 message(s) Application MyApp (Failed to open database), Application MyApp (Retry limit reached)

check_eventlog file=Application bookmark=auto "filter=level = 'error'"
OK: Event log seems fine
```

**Show the raw event XML (useful when working out what to filter on):**

```
check_eventlog file=Application "filter=id = 1000" "detail-syntax=${xml}" show-all
CRITICAL: 1 message(s) <Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='Application Error'/>...
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_eventlog --arguments "file=System" --arguments "filter=level = 'error'"
OK: Event log seems fine
```
