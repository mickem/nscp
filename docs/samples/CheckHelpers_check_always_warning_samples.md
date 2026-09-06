**Force a check's result to WARNING:**

The wrapped check's message survives; only the status is replaced.

```
check_always_warning check_critical "message=Nightly report generated"
WARNING: Nightly report generated
```

**With a real check:**

```
check_always_warning check_drivesize "crit=used > 50%"
WARNING: CRITICAL /opt/claude-code: 202.746MB/229.949MB used, /opt/env-runner: 29.777MB/46.227MB used
'/ used'=8.27045GB;201.57782;125.98613;0;251.97227 '/ used %'=3%;80;50;0;100 '/opt/claude-code used'=202.74609MB;183.95937;114.9746;0;229.94921 '/opt/claude-code used %'=88%;80;50;0;100
```

Performance data passes through unchanged, so the numbers are still graphed even
though the status has been overridden.

**A command that does not exist is also reported as WARNING:**

The override is unconditional, so a broken configuration hides behind it. Only
the message says what went wrong.

```
check_always_warning check_no_such_command
WARNING: Unknown command(s): check_no_such_command
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_always_warning --arguments check_drivesize
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
```
