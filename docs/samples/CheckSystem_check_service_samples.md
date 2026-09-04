##### Windows

**Default check:**

```
check_service
OK all services are ok.
```

**Excluding services using exclude**::

```
check_service "exclude=clr_optimization_v4.0.30319_32"  "exclude=clr_optimization_v4.0.30319_64"
WARNING: gupdate=stopped (auto), Net Driver HPZ12=stopped (auto), NSClientpp=stopped (auto), nscp=stopped (auto), Pml Driver HPZ12=stopped (auto), SkypeUpdate=stopped (auto), sppsvc=stopped (auto)
```

**Show all service by changing the syntax**::

```
check_service "top-syntax=${list}" "detail-syntax=${name}:${state}"
AdobeActiveFileMonitor10.0:running, AdobeARMservice:running, AdobeFlashPlayerUpdateSvc:stopped, ..., WwanSvc:stopped
```

**Excluding services using the filter**::

```
check_service "filter=start_type = 'auto' and name not in ('Bonjour Service', 'Net Driver HPZ12')"
AdobeActiveFileMonitor10.0: running, AdobeARMservice: running, AMD External Events Utility: running,  ... wuauserv: running
```

**Exclude versus filter**::

You can use both exclude and filter to exclude services the befnefit of exclude is that it is faster with the obvious drawback that it only works on the service name.
The upside to filters are that they are richer in terms of functionality i.e. substring matching (as below).

Regular check
```
check_service
CRITICAL: CRITICAL: nfoo=stopped (auto), nscp=stopped (auto), nscp2=stopped (auto), ...
```

Excluding nfoo service with exclude:
```
check_service exclude=nfoo
CRITICAL: CRITICAL: nscp=stopped (auto), nscp2=stopped (auto), ...
```

Excluding nscp2 with substring like matching filter:
```
check_service exclude=nfoo "filter=name not like 'nscp'"
CRITICAL: CRITICAL: ...
```


Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_service
WARNING: DPS=stopped (auto), MSDTC=stopped (auto), sppsvc=stopped (auto), UALSVC=stopped (auto)
```

**Check that a service is not started**::

```
check_service service=nscp "crit=state = 'started'" warn=none
```

**Dashboard rollup with `summary` (aggregate state-count perfdata)**::

Adding `summary` emits per-state counts across all enumerated services as
performance data, so a dashboard gets running/stopped/paused/pending/total
rollups without a custom `top-syntax`:

```
check_service summary "filter=none"
OK: All 214 service(s) are ok.
'running_services'=118 'stopped_services'=94 'paused_services'=0 'pending_services'=2 'service_count'=214
```

The counts cover every matched service regardless of the warning/critical
filter, so the rollup is stable even when the check itself is OK.

##### Linux

**Check all services (the default watches enabled units for failures):**

```
check_service
OK: All 42 service(s) are ok.
```

**Check one service by name:**

```
check_service service=cron
OK: All 1 service(s) are ok.
```

**Show the mapped state, raw systemd state and vendor preset:**

```
check_service service=cron "top-syntax=${list}" "detail-syntax=${name}=${state} active=${active} preset=${preset}"
cron=running active=active preset=enabled
```

**A failed or stopped enabled service is CRITICAL:**

```
check_service service=nginx
CRITICAL: nginx=stopped
```

**Only alert on a specific service being down:**

```
check_service service=ssh "crit=state != 'running'"
OK: All 1 service(s) are ok.
```

**Alert on a service using too much memory (process metrics):**

```
check_service service=mysql "warn=rss > 1G" "crit=rss > 2G" "detail-syntax=${name} rss=${rss} cpu=${cpu}%"
OK: All 1 service(s) are ok.
```

**Check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_service --argument "service=docker"
OK: All 1 service(s) are ok.
```
