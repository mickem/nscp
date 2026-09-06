**Pass through unchanged (no mappings given):**

```
check_negate command=check_critical
CRITICAL: No message
```

**Invert a check — but only in one direction at a time:**

```
check_negate command=check_critical critical=ok
OK: No message
```

`ok=critical critical=ok` looks like a swap and is not one. The mappings are
applied in sequence to the value as it is rewritten, in the order OK, WARNING,
CRITICAL, UNKNOWN — so an OK result is rewritten to CRITICAL by the first rule
and back to OK by the third:

```
check_negate command=check_critical ok=critical critical=ok
OK: No message
```

Both report OK here only because the input was CRITICAL, which the third rule
maps to OK. An OK input takes the first rule to CRITICAL and then the third
straight back to OK, so the inversion never fires in that direction.

Map only the direction you need, and put it on the state the wrapped check
actually returns in the case you want to alert on.

**Downgrade CRITICAL to WARNING, leaving everything else alone:**

```
check_negate command=check_drivesize critical=warning
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
```

**Pass arguments to the wrapped check:**

`arguments=` (`-a`) is repeatable; each one is handed to the wrapped command.

```
check_negate command=check_drivesize "arguments=crit=used > 50%" critical=warning
WARNING: CRITICAL /opt/claude-code: 202.746MB/229.949MB used, /opt/env-runner: 29.777MB/46.227MB used
```

**Treat UNKNOWN as CRITICAL:**

Useful where an UNKNOWN result — a missing counter, an unreachable service — is
just as actionable as a failure, and you do not want it filtered out by a
notification rule that ignores UNKNOWN.

```
check_negate command=check_no_such_command unknown=critical
CRITICAL: Unknown command(s): check_no_such_command
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_negate --arguments "command=check_drivesize" --arguments "critical=warning"
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
```
