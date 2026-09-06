**Run two checks and return the worst status:**

Each check is one `command=` argument holding the whole command line. The result
carries one line per wrapped check.

```
check_multi "command=check_ok message=first" "command=check_warning message=second"
WARNING: first
WARNING: , second
```

**Shape the combined message with `separator` and `prefix`:**

```
check_multi "command=check_ok message=a" "command=check_ok message=b" "separator= | " "prefix=results: "
OK: results: a
OK:  | b
```

**Combine real checks into one service:**

```
check_multi "command=check_drivesize" "command=check_ok message=second"
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.28236GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100
WARNING: , second
```

The performance data of every wrapped check is merged into the result, so all
the numbers are still graphed against the one service.

**Status escalation:**

The worst status wins, in the order OK < WARNING < CRITICAL < UNKNOWN — so a
single check that returns UNKNOWN makes the whole result UNKNOWN even if the
others are merely CRITICAL.

```
check_multi "command=check_critical" "command=check_no_such_command"
UNKNOWN: No message
UNKNOWN: , Unknown command(s): check_no_such_command
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_multi --arguments "command=check_drivesize" --arguments "command=check_uptime"
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used, uptime: 5d 03:14h
```
