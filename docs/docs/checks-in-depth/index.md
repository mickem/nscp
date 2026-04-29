# Checks In Depth

All NSClient++ checks share the same engine for filtering, thresholds, and output formatting. Understanding this engine once gives you the skills to configure *any* check.

!!! tip "New to NSClient++?"
    Start with the [Quick Start guide](../quick-start.md) or a [Monitoring Scenario](../scenarios/index.md) before diving into the internals.

---

## The Check Engine

Every check command (e.g., `check_cpu`, `check_drivesize`, `check_service`) works the same way:

1. **Collect** — gather a list of items (CPU cores, drives, services, etc.)
2. **Filter** — keep only items that match the `filter` expression
3. **Evaluate** — test each item against the `warn` and `crit` expressions
4. **Format** — format the message using `top-syntax` and `detail-syntax`
5. **Return** — return the worst status, the formatted message, and performance data

This shared engine means the same concepts apply everywhere. Learn them once, use them in all checks.

---

## Section Overview

| Page | What you will learn |
|---|---|
| [Filters and Expressions](filters.md) | How to select which items are included in a check |
| [Thresholds](thresholds.md) | How to set warning and critical conditions |
| [Output Syntax](syntax.md) | How to control the message text |
| [Performance Data](performance-data.md) | How to customise performance data for graphing |

---

## Quick Example: `check_cpu`

Here is the same check with progressively more customisation to show how all the pieces fit:

**Default (everything automatic):**

```
check_cpu
OK: CPU load is ok.
'total 5m'=2%;80;90 'total 1m'=5%;80;90 'total 5s'=11%;80;90
```

**Custom thresholds:**

```
check_cpu "warn=load > 50" "crit=load > 70"
OK: CPU load is ok.
'total 5m'=2%;50;70 'total 1m'=5%;50;70 'total 5s'=11%;50;70
```

**Include per-core data (change filter):**

```
check_cpu filter=none "warn=load > 80" "crit=load > 90"
```

**Custom message text:**

```
check_cpu "top-syntax=%(status): CPU usage is %(list)" "detail-syntax=%(time) avg: %(load)%"
OK: CPU usage is 5m avg: 2%, 1m avg: 5%, 5s avg: 11%
```

---

## Getting Defaults for Any Check

Every check has built-in defaults. To see what they are:

```
check_cpu show-default
```

Output:

```
"filter=core = 'total'" "warning=load > 80" "critical=load > 90" "empty-state=ignored" "top-syntax=${status}: ${problem_list}" "ok-syntax=%(status): CPU load is ok." "detail-syntax=${time}: ${load}%" "perf-syntax=${core} ${time}"
```

This is useful when you want to change one thing without affecting the rest — just take the defaults and modify the one option you care about.

!!! warning
    Do not copy all defaults into your configuration unnecessarily. Default values exist for a reason; adding them explicitly means your configuration may break if the defaults change in a future version.

---

## Getting Help for Any Check

Every check has built-in help:

```
check_cpu help
```

The help includes all available options, filter keywords, and example usage.

---

## Next: Filters

Continue to [Filters and Expressions](filters.md) →
