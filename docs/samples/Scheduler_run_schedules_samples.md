Given this configuration:

```ini
[/modules]
CheckSystem = enabled
CheckHelpers = enabled
Scheduler = enabled
NSCAClient = enabled

[/settings/scheduler/schedules/default]
channel  = NSCA
interval = 1h
report   = all

[/settings/scheduler/schedules]
cpu        = check_cpu
host_check = check_ok
```

**Run every configured schedule now and submit the results:**

```
nscp client --boot --query run_schedules
Ran 2 schedule(s): cpu, host_check
```

**Run a single schedule (repeat `schedule=` for more than one):**

```
nscp client --boot --query run_schedules schedule=cpu
Ran 1 schedule(s): cpu
```

The `-a` / `--argument` spelling works too, which is what you want from a batch
file or a script:

```
nscp client --boot --query run_schedules --argument schedule=cpu
Ran 1 schedule(s): cpu
```

**A schedule that does not exist is reported, not silently skipped:**

```
nscp client --boot --query run_schedules schedule=nosuchcheck
No such schedule: nosuchcheck (available: cpu, host_check)
```

**From the interactive test prompt:**

```
nscp test
...
run_schedules
OK: Ran 2 schedule(s): cpu, host_check
```

**Over REST**, to wire a "push my results now" button to the agent:

```
curl -k -u admin:<password> "https://<agent>:8443/api/v2/queries/run_schedules/commands/execute?schedule=cpu"
{"command":"run_schedules","result":0,"lines":[{"message":"Ran 1 schedule(s): cpu","perf":{}}]}
```

**Via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command run_schedules
OK: Ran 2 schedule(s): cpu, host_check
```
