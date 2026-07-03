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

Keywords:

| Keyword       | Description                                                        |
|---------------|-------------------------------------------------------------------|
| `name`/`service` | Unit name                                                      |
| `desc`        | Unit description                                                  |
| `state`       | Normalised state: `running`, `stopped`, `starting`, `oneshot`, `static`, `unknown` |
| `active`      | Raw systemd `ActiveState` (`active`, `inactive`, `failed`)        |
| `sub_state`   | Raw systemd `SubState` (`running`, `dead`, `exited`, …)           |
| `preset`      | Vendor preset (`enabled`, `disabled`)                             |
| `start_type`  | Configured start type (`enabled`, `disabled`, `static`, `masked`) |
| `pid`         | Main process id                                                   |
| `rss` / `vms` | Resident / virtual memory of the main process, in bytes          |
| `cpu`         | Lifetime-average CPU percent of the main process                 |
| `tasks`       | Number of tasks (cgroup) for the unit                            |
| `created`     | Unix timestamp when the main process started                     |
| `age`         | Seconds since the main process started                           |
