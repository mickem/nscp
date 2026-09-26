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

An `enabled` unit that has **failed** is therefore CRITICAL.

A unit that is merely **stopped**, however, never reaches that threshold: the
default filter `active != 'inactive'` excludes it before the critical expression
is evaluated. With nothing left to match, the check falls to its empty state,
which is `unknown`:

```
check_service service=nginx
UNKNOWN: No services found
```

`service=<name>` (repeatable) narrows which units are *enumerated*; it does not
bypass the filter. To alert on a unit being stopped rather than failed, widen
the filter so inactive units are considered:

```
check_service service=nginx filter=none "crit=state != 'running'"
```

`exclude=` drops units by name, and `state=` (`all`, `active`, `inactive`,
`failed`) restricts the enumeration before filtering.

##### macOS

`check_service` inspects **launchd** jobs in the system domain, which is what
a daemon sees and the counterpart of systemd's system services. `service=`
takes a job label (`service=com.apple.logd`); there is no `.service` suffix.
The job list comes from `launchctl print system`, the overrides from
`launchctl print-disabled system`, and a check by name also reads
`launchctl print system/<label>`.

Each job is mapped onto the same fields, so the default thresholds read the
same way:

| launchd job                                    | `active`   | `sub_state` | `state`   |
|------------------------------------------------|------------|-------------|-----------|
| has a pid                                      | `active`   | `running`   | `running` |
| last exited with a non-zero code               | `failed`   | `failed`    | `stopped` |
| idle, and launchd starts it on demand          | `inactive` | `dead`      | `static`  |
| idle otherwise                                 | `inactive` | `dead`      | `stopped` |

`start_type` is `disabled` for a job disabled by an override, `enabled` for one
that runs at load or is kept alive, and `on-demand` for the rest. The
distinction needs the job's own properties, so it is made for a check by name;
`service=*` lists every job as `enabled` or `disabled`. A negative last status
is the signal launchd stopped an idle job with, and does not count as a
failure. `preset` has no launchd counterpart and is empty.

`rss`, `vms`, `cpu`, `tasks` (the thread count), `created` and `age` come from
libproc. The first four need the job's task info, which the unprivileged agent
only has for its own processes; for any other job they read 0, and
`has_metrics` is `false`.
