**List processes first seen in the last few minutes (needs the collector):**

```
check_process_history_new
OK: No new processes found.
```

**When the collector is not enabled it tells you exactly what to set:**

```
check_process_history_new
Process history is not enabled (set 'process history = true' under /settings/system/unix)
```

**Widen the "recently started" window to one hour:**

```
check_process_history_new time=1h
OK: No new processes found.
```

**Alert when any new process appears (e.g. detect unexpected launches):**

```
check_process_history_new time=10m "warn=count > 0"
WARNING: /usr/bin/rogue (first seen: 1720000000)
```

**Watch for a specific executable starting:**

```
check_process_history_new "crit=exe = '/usr/bin/nmap'"
OK: No new processes found.
```
