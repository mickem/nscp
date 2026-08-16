**Check that the machine is following a time source (Windows)**

The default is critical when the machine is not synchronizing at all and warning
once the computed offset passes one second.

```
check_w32time
L        cli OK: synchronizing with dc01.corp.example.com (offset 3ms)|'w32time_offset'=3ms;1000;30000
```

```
check_w32time
L        cli CRITICAL: the Windows Time service is stopped (start type demand)
```

```
check_w32time
L        cli CRITICAL: not synchronizing: falling back to Local CMOS Clock
```

```
check_w32time
L        cli CRITICAL: not synchronizing: no time source in use (configured: time.windows.com)|'w32time_offset'=0ms;1000;30000
```

**Show the service state, configuration and source**

```
check_w32time warning=none critical=none "top-syntax=${status}: ${list}" "detail-syntax=svc=${service_state}/${start_type} type=${sync_type} src=${source} (${source_from}) peers=${peers}"
L        cli OK: svc=stopped/demand type=NTP src=time.windows.com (configuration) peers=time.windows.com
```

`source_from` says where the source came from: `service` when the running
service was asked what it is actually following, `configuration` when it could
not be asked and the configured peers are shown instead. The verdict is worded
to match — "synchronizing with X" only when the service confirmed it, and
"configured to synchronize with X" when that is all we know.

```
check_w32time warning=none critical=none "top-syntax=${list}" "detail-syntax=src=[${source}] from=${source_from} local=${local_clock} sync=${synchronized} srcs=${time_sources} off=${offset} delay=${delay}"
L        cli src=[time.windows.com] from=configuration local=0 sync=0 srcs=0 off=0 delay=31
```

**Watch a domain member's time hierarchy**

`local_clock` is the signal that a domain member has lost its hierarchy and is
free-running: it keeps answering, but its clock is no longer anchored to
anything, which breaks Kerberos once it drifts past five minutes.

```
check_w32time "critical=local_clock = 1 or sync_type = 'NoSync' or running = 0"
L        cli OK: synchronizing with dc01.corp.example.com (offset 12ms)
```

**Alert on drift only**

```
check_w32time "warning=offset > 500" "critical=offset > 5000"
L        cli WARNING: synchronizing with time.windows.com (offset 812ms)|'w32time_offset'=812ms;500;5000
```

**Report how long ago the clock was last validated**

```
check_w32time "warning=last_sync_age > 86400" "critical=none" "top-syntax=${status}: ${list}" "detail-syntax=last sync ${last_sync} (${last_sync_age}s ago)"
L        cli OK: last sync 2026-08-15 21:28:41 (49654s ago)|'w32time_last_sync'=49654s;86400;0
```

**Values the service has not measured read as `unknown`**

The "Windows Time Service" counters only carry data while the service is
running; until then `offset`, `delay`, `frequency_adjustment` and `time_sources`
render as `unknown`, compare false against every number and emit no perfdata.

```
check_w32time "warning=none" "critical=none" "top-syntax=${list}" "detail-syntax=off=${offset} delay=${delay} freq=${frequency_adjustment} srcs=${time_sources}"
L        cli off=unknown delay=unknown freq=unknown srcs=unknown
```

```
check_w32time "critical=offset = 'unknown'"
L        cli CRITICAL: the Windows Time service is stopped (start type demand)
```

**A workgroup client, where W32Time is trigger-started**

Windows starts the time service on demand on a machine that is not domain
joined, so it is stopped most of the time. Check the configuration and the age
of the last good synchronization there instead of the service state.

```
check_w32time "critical=sync_type = 'NoSync'" "warning=last_sync_age > 604800"
L        cli OK: the Windows Time service is stopped (start type demand)|'w32time_last_sync'=50036s;604800;0
```
