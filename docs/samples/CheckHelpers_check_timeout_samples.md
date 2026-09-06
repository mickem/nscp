**Run a check with a deadline:**

A check that answers in time returns its own result unchanged.

```
check_timeout command=check_drivesize timeout=1
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.27112GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100
```

**Pass arguments to the wrapped check:**

```
check_timeout command=check_drivesize timeout=10 "arguments=crit=used > 50%"
CRITICAL: CRITICAL /opt/claude-code: 202.746MB/229.949MB used, /opt/env-runner: 29.777MB/46.227MB used
```

**What a timeout looks like:**

Here `slow_thing` is an external script that sleeps for 30 seconds:

```
check_timeout command=slow_thing timeout=3
UNKNOWN: Thread failed to return within given timeout
```

The wrapped check is *detached*, not killed, so it goes on running in the
background — only its result is thrown away.

**Override the status of a check that does finish:**

`return=` applies only on success; it cannot turn a timeout into an OK.

```
check_timeout command=check_critical timeout=5 return=ok
OK: No message
```

**Guarding a slow remote check over NRPE:**

Set the timeout comfortably below the monitoring server's own check timeout so
you get a message explaining the hang rather than a bare plugin timeout.

```
check_nrpe --host 192.168.56.103 --command check_timeout --arguments "command=check_mssql" --arguments "timeout=20"
UNKNOWN: Thread failed to return within given timeout
```
