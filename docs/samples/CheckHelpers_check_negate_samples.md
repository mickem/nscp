**Pass through unchanged (no mappings given):**

```
check_negate command=check_critical
CRITICAL: No message
```

**Invert a check — swap OK and CRITICAL:**

The classic use: alert when something *is* present rather than when it is
missing.

```
check_negate command=check_critical ok=critical critical=ok
OK: No message
```

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
