#### About `check_service`

`check_service` reports the state of the machine's services. `state` is
normalised across platforms so the same `warning=` / `critical=` expressions
read the same way on Windows and Linux; the platform-native fields are exposed
alongside it.

##### Windows

Enumerates the Service Control Manager. Two helper functions make the
"is this service actually fine" question expressible in a filter:

##### `state_is_ok`

Helper function that checks if the state of a service is "OK". It returns `True` if the state is "OK" and `False` otherwise.
This can be used in filter expressions to warn about services that are not running properly.

| Configured            | State     | exit_code | Result of `state_is_ok` |
|-----------------------|-----------|-----------|-------------------------|
| auto-start            | running   | any       | ✅ ok                    |
| delayed auto-start    | stopped   | any       | ✅ ok                    |
| auto-start + triggers | stopped   | any       | ✅ ok                    |
| auto-start            | stopped   | 0         | ✅ ok                    |
| auto-start            | stopped   | non zero  | ❌ not ok                |
| demand-start          | any state | any       | ✅ ok                    |

##### `state_is_perfect`

Helper function that checks if the state of a service is "perfect". It returns `True` if the state is "perfect" and `False` otherwise.
This can be used in filter expressions to warn about services that are not running perfectly.

| Configured            | State     | Result of `state_is_perfect` |
|-----------------------|-----------|------------------------------|
| auto-start            | running   | ✅ perfect                    |
| auto-start            | stopped   | ❌ not perfect                |
| auto-start + triggers | stopped   | ✅ perfect                    |
| demand-start          | any state | ✅ perfect                    |
| disabled              | stopped   | ✅ perfect                    |

##### Linux

`check_service` inspects **systemd** units (via `systemctl show`). It
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
