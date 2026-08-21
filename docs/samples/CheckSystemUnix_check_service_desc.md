#### About `check_service` (Linux)

On Linux `check_service` inspects **systemd** units (via `systemctl show`). It
maps each unit's raw systemd state to a normalised `state` keyword so thresholds
read the same way as on Windows, and also exposes the raw systemd fields and the
main process's resource usage.

By default it looks at units that are *not* inactive
(`filter = active != 'inactive'`) and treats a unit as **critical** when it is
not in a healthy state and is not deliberately disabled:

```
critical = ( state not in ('running', 'oneshot', 'static') or active = 'failed' ) and preset != 'disabled'
```

This means a stopped-but-`disabled` unit is ignored, while an `enabled` unit
that has failed or stopped is CRITICAL. Pass `service=<name>` (repeatable) to
check specific units, or override `filter=` / `warning=` / `critical=`.
