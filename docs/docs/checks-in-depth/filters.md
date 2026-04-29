# Filters and Expressions

Filters control which items are included in a check. The filter language is the same across all NSClient++ checks and is modelled after SQL `WHERE` clauses.

!!! tip "Beginner?"
    See a filter in action in the [Windows Server Health scenario](../scenarios/windows-server-health.md) or [Event Log scenario](../scenarios/event-log.md) before reading this page.

---

## What Is a Filter?

When a check runs, it collects a list of items. For `check_cpu` that is CPU cores. For `check_service` that is Windows services. For `check_eventlog` that is event log entries.

A **filter** is an expression that is evaluated for each item. Items where the expression is `true` are **included** in the check; items where it is `false` are **excluded**.

If you do not specify a filter, NSClient++ applies a default filter. To see what it is:

```
check_cpu show-default
"filter=core = 'total'" ...
```

---

## Filter Syntax

A filter expression takes the form:

```
keyword operator value
```

For example:

```
check_cpu "filter=core = 'total'"
```

This keeps only the item where the `core` keyword equals `'total'` (i.e., the aggregate total CPU load, not individual cores).

### Combining expressions

Use `and` and `or` to combine multiple conditions:

```
check_cpu "filter=load > 5 or core = 'total'"
```

Use `not` to negate:

```
check_service "filter=start_type = 'auto' and not name like 'clr_optimization'"
```

### Disabling the default filter

To include all items (no filtering):

```
check_cpu filter=none
```

---

## Operators

| Symbol | Safe alternative | Meaning | Example |
|---|---|---|---|
| `=` | `eq` | Equals | `core = 'total'` |
| `!=` | `ne` | Not equals | `state != 'running'` |
| `>` | `gt` | Greater than | `load > 80` |
| `<` | `lt` | Less than | `free < 10` |
| `>=` | `ge` | Greater than or equal | `count >= 1` |
| `<=` | `le` | Less than or equal | `age <= 60` |
| `like` | `like` | Substring match | `name like 'sql'` |
| `regexp` | `regexp` | Regular expression | `name regexp '^sql.*'` |
| `not` | `not` | Negate | `not state = 'running'` |
| `and` | `and` | Logical AND | `load > 5 and core = 'total'` |
| `or` | `or` | Logical OR | `load > 5 or core = 'total'` |
| `'...'` | `str(...)` | String literal | `state = 'running'` |

!!! note
    The **safe** alternatives (e.g., `gt` instead of `>`) avoid shell escaping problems when passing arguments through NRPE or Nagios. They are identical to the regular operators.

---

## Keywords

Each check has its own set of filter keywords. Common ones for all checks:

| Keyword | Meaning |
|---|---|
| `count` | Number of items matching the filter |
| `total` | Total number of items (before filtering) |
| `ok_count` | Number of items in OK state |
| `warn_count` | Number of items in warning state |
| `crit_count` | Number of items in critical state |
| `problem_count` | Number of items in warning or critical state |
| `status` | The current result status (`OK`, `WARNING`, etc.) |

### `check_cpu` keywords

| Keyword | Meaning |
|---|---|
| `core` | Core name (`'total'`, `'core 0'`, `'core 1'`, …) |
| `core_id` | Core identifier (with underscore: `'core_0'`) |
| `load` | CPU load percentage for this core and time window |
| `idle` | Idle percentage |
| `kernel` | Kernel time percentage |
| `time` | Time window (`'5m'`, `'1m'`, `'5s'`) |

Use `check_cpu help` or any check's `help` option to see its full keyword list.

---

## Practical Examples

### CPU: only check total, not individual cores

```
check_cpu "filter=core = 'total'"
```

### CPU: check cores that are actually doing work

```
check_cpu filter=none "filter=load > 5 or core = 'total'"
```

### Service: check only auto-start services

```
check_service "filter=start_type = 'auto'"
```

### Service: exclude a specific service by name

```
check_service "filter=start_type = 'auto' and name != 'Spooler'"
```

### Event log: only errors, not warnings

```
check_eventlog "filter=level = 'error'"
```

### Event log: find specific provider and event ID

```
check_eventlog "filter=provider = 'Microsoft-Windows-Security-SPP' and id = 903"
```

### Disk: only fixed and network drives

```
check_drivesize drive=* "filter=type in ('fixed', 'remote')"
```

---

## The `in` Operator

The `in` operator checks whether a value is in a list:

```
check_eventlog "filter=level in ('error', 'critical')"
check_drivesize "filter=type in ('fixed', 'remote')"
```

---

## Size Units in Expressions

For checks that deal with sizes (memory, disk), you can use unit suffixes:

| Suffix | Meaning |
|---|---|
| `k` | Kilobytes (×1024) |
| `m` | Megabytes (×1024²) |
| `g` | Gigabytes (×1024³) |
| `t` | Terabytes (×1024⁴) |

```
check_memory "warn=free < 4g"
check_drivesize "crit=free < 500m"
check_process process=myapp.exe "warn=working_set > 200m"
```

---

## Next: Thresholds

Filters control which items are included. [Thresholds](thresholds.md) control which included items trigger a warning or critical status.

Continue to [Thresholds](thresholds.md) →
