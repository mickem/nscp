# Thresholds (warn and crit)

Thresholds control which items trigger a WARNING or CRITICAL status. They use the same expression language as [filters](filters.md), but instead of selecting which items to *include*, they determine which items trigger an *alert*.

!!! tip "Beginner?"
    If you haven't read [Filters and Expressions](filters.md) yet, start there — thresholds use the same syntax.

---

## How Thresholds Work

After filtering, NSClient++ evaluates the `warn` and `crit` expressions against each item:

- If any item matches `crit` → the check returns **CRITICAL**
- If any item matches `warn` (and nothing matched `crit`) → the check returns **WARNING**
- If no item matches either → the check returns **OK**

The threshold expressions use exactly the same keyword and operator syntax as filters.

---

## Setting Thresholds

```
check_cpu "warn=load > 80" "crit=load > 90"
```

```
check_memory "warn=free < 20%" "crit=free < 10%"
```

```
check_drivesize "warn=free < 15%" "crit=free < 5%"
```

---

## Default Thresholds

Every check comes with sensible defaults. To see them:

```
check_cpu show-default
"warning=load > 80" "critical=load > 90" ...
```

If you only need to change one threshold, just specify that one:

```
check_cpu "warn=load > 60"    ; crit stays at the default 90
```

### Disabling a threshold

To completely disable warning or critical thresholds:

```
check_cpu warning=none
check_cpu critical=none
check_cpu warning=none critical=none
```

---

## Threshold Expressions

Threshold expressions are the same as filter expressions. You can use:

- Comparison operators: `>`, `<`, `>=`, `<=`, `=`, `!=` (or their safe alternatives `gt`, `lt`, `ge`, `le`, `eq`, `ne`)
- Logical operators: `and`, `or`, `not`
- String comparisons and substring matching

### Example: composite threshold

Warn if CPU load is high **or** kernel time is high:

```
check_cpu filter=none "warn=kernel > 10 or load > 80" "crit=load > 90"
```

Warn if memory is under 4 GB free **and** the machine has more than 16 GB total:

```
check_memory "warn=free < 4g and size > 16g"
```

---

## Aggregate Keywords

Thresholds can also apply to aggregate values across all items. These keywords work in both `warn`/`crit` and `filter`:

| Keyword | Meaning |
|---|---|
| `count` | Number of items matching the filter |
| `ok_count` | Number of items in OK state |
| `warn_count` | Number of items in WARNING state |
| `crit_count` | Number of items in CRITICAL state |
| `problem_count` | Number of items in WARNING or CRITICAL state |

**Example: alert if more than 3 services are stopped:**

```
check_service "crit=problem_count > 3"
```

**Example: alert if any event log entry was written in the past hour:**

```
check_eventlog scan-range=-1w "crit=written > -1h"
```

---

## Time Units in Thresholds

For checks with time-based values, you can use time unit suffixes:

| Suffix | Meaning |
|---|---|
| `s` | Seconds |
| `m` | Minutes |
| `h` | Hours |
| `d` | Days |
| `w` | Weeks |

**Example: alert if the server was rebooted in the last day:**

```
check_uptime "warn=uptime < 1d" "crit=uptime < 1h"
```

**Note:** Past times are expressed as **negative** values in NSClient++ time expressions:

```
check_eventlog scan-range=-24h "crit=written > -1h"
```

Here `-1h` means "written more recently than 1 hour ago" (i.e., in the last hour).

---

## Performance Data and Thresholds

When you set `warn` and `crit` thresholds, NSClient++ automatically includes the threshold values in the performance data output:

```
check_cpu "warn=load > 80" "crit=load > 90"
'total 5m'=2%;80;90 'total 1m'=5%;80;90 'total 5s'=11%;80;90
```

The format is `'metric'=value;warn;crit;min;max`.

If you remove all thresholds, performance data is also removed:

```
check_cpu warning=none critical=none
'total 5m'= 'total 1m'= 'total 5s'=
```

To get performance data without thresholds, use `perf-config`. See [Performance Data](performance-data.md).

---

## Next: Output Syntax

Thresholds control the *status*. [Output Syntax](syntax.md) controls the *message text* that appears alongside the status.

Continue to [Output Syntax](syntax.md) →
