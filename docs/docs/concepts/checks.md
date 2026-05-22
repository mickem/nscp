# Checks In Depth

All NSClient++ checks — `check_cpu`, `check_drivesize`, `check_service`, `check_eventlog`, and dozens more — share **one
common engine**. Once you understand it, you can configure any check.

This page walks through that engine from the ground up. Read it top-to-bottom the first time; later, jump to the section
you need.

<!-- @formatter:off -->
!!! tip "New to NSClient++?"
    Try the [Quick Start](../quick-start.md) or a [Monitoring Scenario](../scenarios/index.md) first to get a feel for what
    checks look like in practice.
<!-- @formatter:on -->

---

## 1. How a Check Works

Every check follows the same five steps:

```mermaid
flowchart LR
    A[Collect items] --> B[Filter]
    B --> C[Evaluate<br/>warn / crit]
    C --> D[Format<br/>top / detail-syntax]
    D --> E[Return<br/>status + perfdata]
```

1. **Collect** — gather a list of items (CPU cores, drives, services, event log entries…).
2. **Filter** — keep only items matching the `filter` expression.
3. **Evaluate** — test each kept item against `warn` and `crit`.
4. **Format** — build the message using `top-syntax` and `detail-syntax`.
5. **Return** — return the worst status, the message, and the performance data.

Each step has its own option. The four most important are:

| Option                                       | Purpose                          |
|----------------------------------------------|----------------------------------|
| `filter`                                     | Which items are included         |
| `warn` / `crit`                              | Which items trigger an alert     |
| `top-syntax` / `detail-syntax` / `ok-syntax` | What the message looks like      |
| `perf-config` / `perf-syntax`                | What performance data looks like |

---

## 2. Trying It Out (Test Mode)

The fastest way to learn the engine is to drive a check yourself. NSClient++ ships with a built-in
**test shell** that runs the same modules the service does, but interactively, with all log output
visible.

**2.1. Start the shell**

```
nscp test --settings dummy
...
L     client Enter command to inject or exit to terminate...
```

`--settings dummy` skips your real configuration so nothing on disk surprises you. Type `exit` to leave.

Trace and debug log lines are hidden by default. Add `--log debug` (or `--log trace` for maximum
verbosity, including incoming requests and child-process spawn/exit) when you need to see them:

```
nscp test --settings dummy --log debug
```

**2.2. Load a module**

Out of the box the shell has no checks loaded. Each check lives in a module — load it once per
session, then run the check:

```
load CheckSystem
```

**2.3. Run a check**

Now we can run the check:

```
check_cpu
L     client OK: CPU Load ok
L     client  Performance data: 'total 5m'=0%;80;90 'total 1m'=1%;80;90 'total 5s'=11%;80;90
```

Options are passed as `keyword=value`, with quotes when the value contains spaces or shell-meta
characters. Some keywords are flags with no value (e.g. `help`, `show-all`, `show-default`):

```
check_cpu filter=none
check_cpu "filter=core = 'total'" "warn=load > 80"
```

**2.4. Two essential helpers: `show-default` and `help`**

Before changing anything, learn these two — they work for **every** check.

`show-default` prints the check's built-in options so you can learn what it does by default and copy
a single line to tweak:

```
check_cpu show-default
"filter=core = 'total'" "warning=load > 80" "critical=load > 90"
"empty-state=ignored" "top-syntax=${status}: ${problem_list}"
"ok-syntax=%(status): CPU load is ok." "detail-syntax=${time}: ${load}%"
"perf-syntax=${core} ${time}"
```

`help` gives the full list of options, filter keywords, and per-check examples:

```
check_cpu help
```

<!-- @formatter:off -->
!!! note
    You'll see `${name}` in the defaults output. That's the older placeholder syntax, still supported
    for compatibility. When you write your own configuration, prefer `%(name)` — see
    [section 5](#5-output-syntax-choosing-the-message-text).

!!! warning
    Don't paste *all* defaults into your config. Defaults can change in newer versions; pinning them removes that benefit.
<!-- @formatter:on -->

**2.5. A worked example**

The same `check_cpu`, with progressively more customisation:

```text
# Defaults
check_cpu
OK: CPU load is ok.
'total 5m'=2%;80;90 'total 1m'=5%;80;90 'total 5s'=11%;80;90

# Custom thresholds
check_cpu "warn=load > 50" "crit=load > 70"
OK: CPU load is ok.
'total 5m'=2%;50;70 ...

# Per-core data instead of just the total
check_cpu filter=none "warn=load > 80" "crit=load > 90"

# Custom message
check_cpu "top-syntax=%(status): CPU usage is %(list)" \
          "detail-syntax=%(time) avg: %(load)%"
OK: CPU usage is 5m avg: 2%, 1m avg: 5%, 5s avg: 11%
```

The rest of this page explains exactly what each of those options does.

---

## 3. Filters — Choosing What to Check

A **filter** is an expression evaluated for each item. Items where it is `true` are included; items where it is `false`
are dropped.

If you don't supply one, the check uses its default (e.g., `check_cpu` defaults to `core = 'total'`).

### Syntax

```
keyword operator value
```

Combine with `and`, `or`, `not`. Disable the default with `filter=none`.

```
check_cpu "filter=core = 'total'"
check_cpu "filter=load > 5 or core = 'total'"
check_service "filter=start_type = 'auto' and not name like 'clr_optimization'"
check_cpu filter=none                       # include everything
```

### Operators

| Symbol            | Safe alias          | Meaning              |
|-------------------|---------------------|----------------------|
| `=`               | `eq`                | Equals               |
| `!=`              | `ne`                | Not equals           |
| `>` `<` `>=` `<=` | `gt` `lt` `ge` `le` | Numeric comparison   |
| `like`            | —                   | Substring match      |
| `regexp`          | —                   | Regular expression   |
| `in`              | —                   | Membership in a list |
| `and` `or` `not`  | —                   | Logical              |
| `'...'`           | `str(...)`          | String literal       |

<!-- @formatter:off -->
!!! note
    Use the **safe aliases** (`gt`, `lt`, …) when passing arguments through NRPE or shells — they avoid `<`/`>` redirection
    problems. The same expression language powers `filter`, `warn`, `crit`, and the `%(...)` placeholders
    in `top-syntax` / `detail-syntax`, so anything you learn here applies everywhere.
<!-- @formatter:on -->

### Common keywords

These are available in *every* check:

| Keyword                                  | Meaning                   |
|------------------------------------------|---------------------------|
| `count`                                  | Items matching the filter |
| `total`                                  | Items before filtering    |
| `ok_count` / `warn_count` / `crit_count` | Items in each state       |
| `problem_count`                          | Warning + critical        |
| `status`                                 | Current overall status    |

Each check adds its own keywords. For `check_cpu`: `core`, `core_id`, `load`, `idle`, `kernel`, `time`. Use
`<check> help` to discover them.

### Practical filter recipes

```shell
# CPU: only the aggregate total
check_cpu "filter=core = 'total'"

# CPU: cores actually doing work, plus the total
check_cpu filter=none "filter=load > 5 or core = 'total'"

# Services: only auto-start, exclude one by name
check_service "filter=start_type = 'auto' and name != 'Spooler'"

# Event log: errors and criticals only
check_eventlog "filter=level in ('error', 'critical')"

# Disk: fixed and network drives
check_drivesize drive=* "filter=type in ('fixed', 'remote')"
```

### Size and time units

| Suffix                      | Size              | Time                                     |
|-----------------------------|-------------------|------------------------------------------|
| `k` / `m` / `g` / `t`       | KB / MB / GB / TB | —                                        |
| `s` / `m` / `h` / `d` / `w` | —                 | seconds / minutes / hours / days / weeks |

```
check_memory "warn=free < 4g"
check_uptime "warn=uptime < 1d"
```

Past times use **negative** values — `-1h` means "less than an hour ago":

```
check_eventlog scan-range=-24h "crit=written > -1h"
```

---

## 4. Thresholds — Choosing What's a Problem

`warn` and `crit` use the **same expression language as filters** — same operators, same keywords,
same `and`/`or`/`not`. The difference is what the expression *decides*:

| Expression  | Per-item question                  | Effect when true                |
|-------------|------------------------------------|---------------------------------|
| `filter`    | Should I include this item at all? | Item is kept (or dropped)       |
| `warn`      | Is this item a warning?            | Overall status becomes WARNING  |
| `crit`      | Is this item critical?             | Overall status becomes CRITICAL |

Aggregation rules:

- Any item matches `crit` → result is **CRITICAL**
- Any item matches `warn` (and none matched `crit`) → **WARNING**
- Otherwise → **OK**

```
check_cpu "warn=load > 80" "crit=load > 90"
check_memory "warn=free < 20%" "crit=free < 10%"
check_drivesize "warn=free < 15%" "crit=free < 5%"
```

### Disabling a threshold

```shell
check_cpu warning=none
check_cpu critical=none
```

### Composite thresholds

```shell
# Warn on high kernel time OR high load
check_cpu filter=none "warn=kernel > 10 or load > 80" "crit=load > 90"

# Warn only on big machines that are tight on memory
check_memory "warn=free < 4g and size > 16g"
```

### Aggregate thresholds

The `count` family of keywords lets you alert on totals rather than individual items:

```shell
# Alert if more than 3 services are stopped
check_service "crit=problem_count > 3"

# Alert if any matching event was written in the last hour
check_eventlog scan-range=-1w "crit=written > -1h"
```

### `empty-state` — when nothing matches

What status to return when the filter selects no items:

| Value      | Meaning                             |
|------------|-------------------------------------|
| `ok`       | Return OK (default for most checks) |
| `warning`  | Return WARNING                      |
| `critical` | Return CRITICAL                     |
| `ignored`  | Suppress the result                 |

```shell
check_service "filter=name = 'NonExistentService'" empty-state=ok
```

---

## 5. Output Syntax — Choosing the Message Text

Three options shape the message. They affect *only* the human-readable text — never the status or perfdata.

| Option          | When applied               | Default purpose           |
|-----------------|----------------------------|---------------------------|
| `top-syntax`    | Always — the whole message | Status + list of problems |
| `detail-syntax` | Per item, inside the list  | Per-item values           |
| `ok-syntax`     | When status is OK          | Brief "all ok" message    |

`check_cpu` defaults (still using the legacy `${...}` form for top-syntax / detail-syntax; written
the recommended way they'd be `top-syntax=%(status): %(problem_list)` and
`detail-syntax=%(time): %(load)%`):

```
"top-syntax=${status}: ${problem_list}"
"ok-syntax=%(status): CPU load is ok."
"detail-syntax=${time}: ${load}%"
```

### Template variables

NSClient++ has two placeholder forms. **Use `%(name)`** — it's the modern, more capable syntax.

| Form         | Status        | When it makes sense                                                       |
|--------------|---------------|---------------------------------------------------------------------------|
| `%(name)`    | **Preferred** | Always. Survives shells/NRPE, supports nested parens and function calls.  |
| `${name}`    | Legacy        | Still works for plain variable references; kept for backwards compatibility. |

Differences in practice:

- **Plain variable references** — both forms work and produce identical output.
- **Function calls** (see [section 6](#6-functions-transforming-values)) — only `%(...)` works. The
  `${...}` form stops at the first `}` and can't capture nested parentheses.
- **Shells and NRPE** — `${...}` is often eaten by Bash and similar shells before it reaches
  NSClient++. `%(...)` passes through untouched.

The rest of this document uses `%(...)` in every example. `${...}` is documented only where it
appears in defaults so you can recognise it in older configs.

Common variables (every check):

| Variable                                       | Meaning                                         |
|------------------------------------------------|-------------------------------------------------|
| `%(status)`                                    | OK / WARNING / CRITICAL / UNKNOWN               |
| `%(list)`                                      | All filtered items, joined with `detail-syntax` |
| `%(problem_list)`                              | Only warning/critical items                     |
| `%(ok_list)` / `%(warn_list)` / `%(crit_list)` | Items in each state                             |
| `%(count)` / `%(problem_count)`                | Item counts                                     |

Per-item variables (in `detail-syntax`) depend on the check — `<check> help` lists them.

### Recipes

```shell
# Show all values, not just problems
check_cpu show-all
# equivalent to:
check_cpu "top-syntax=%(status): %(list)"

# Custom CPU message
check_cpu time=5m \
  "top-syntax=%(status): Cpu usage is %(list)" "detail-syntax=%(load)%"
OK: Cpu usage is 26%

# Custom memory message
check_memory "top-syntax=%(list)" \
  "detail-syntax=%(type) free: %(free) used: %(used) size: %(size)"
page free: 16G used: 7.98G size: 24G, physical free: 4.18G used: 7.8G size: 12G

# Service: name and state for each
check_service "top-syntax=%(list)" "detail-syntax=%(name): %(state)"
```

---

## 6. Functions — Transforming Values

Sometimes a raw value isn't what you want to look at. A counter that returns bytes per second is
unfriendly to read as `4194304`, and a threshold expressed in MB is easier to maintain than one
expressed in 1024-scaled bytes. NSClient++ supports **functions** for these jobs, and the same
function works in both contexts:

- inside `detail-syntax` / `top-syntax` to format a value for display
- inside `warn` / `crit` / `filter` to derive a value for comparison

### Calling a function

A function call looks like a normal function: `name(arg1, arg2, …)`. Arguments can mix variables,
string literals, and numbers. Always wrap the call in `%(...)` when using it inside a syntax
template:

```text
detail-syntax = "Used: %(format_bytes(used))"
warning       = "convert_bytes(used, 'MB') > 500"
filter        = "scale(rate, 1000000) > 100"
```

<!-- @formatter:off -->
!!! warning "Function calls require `%(...)`"
    The legacy `${...}` placeholder stops at the first `}` and can't capture nested parentheses, so
    `${format_bytes(used)}` won't parse. Use `%(...)` for function calls (and prefer it everywhere
    else — see [section 5](#5-output-syntax-choosing-the-message-text)).
<!-- @formatter:on -->

### Built-in functions

These are available wherever the check exposes them — `check_pdh` ships them today; other checks
add their own. Run `<check> help` to see the list for a given check.

| Function                     | Returns | Purpose                                                                  |
|------------------------------|---------|--------------------------------------------------------------------------|
| `format_bytes(value)`        | string  | Auto-scaled human-readable bytes — `4194304 → "4MB"` (1024-based)        |
| `format_bytes(value, 'MB')`  | string  | Fixed unit. Units: `B`, `K`/`KB`, `M`/`MB`, `G`/`GB`, `T`/`TB`           |
| `convert_bytes(value, 'MB')` | float   | Numeric value in the named unit — use in thresholds                      |
| `scale(value, divisor)`      | float   | Divide by an arbitrary divisor — for decimal units (Mbps, etc.)          |

### Recipes

```text
# Show raw bytes as MB/GB/etc. in the message
check_pdh counter=disk_bytes \
  "detail-syntax=%(alias) is %(format_bytes(value))"

# Threshold in MB, display in human-friendly units
check_pdh counter=memory_bytes \
  "warning=convert_bytes(value, 'MB') > 500" \
  "detail-syntax=%(alias) = %(format_bytes(value))"

# Network rates — Mbps is decimal (10⁶), use scale()
check_pdh counter=bytes_per_sec \
  "detail-syntax=Speed = %(scale(value, 1000000)) Mbps"

# Combine functions and plain variables in one template
check_pdh counter=disk_writes \
  "detail-syntax=%(counter): %(format_bytes(value)) (%(value) raw bytes)"
```

### How it composes with the rest of the language

Functions are first-class values, so the result composes with operators, `and`/`or`, and `not`:

```text
# Warn if used MB AND free MB are both at risk
check_disk \
  "warn=convert_bytes(used, 'MB') > 800 and convert_bytes(free, 'MB') < 100"

# Critical when ANY of two derived values is over budget
check_pdh ... \
  "crit=scale(read_rate, 1000000) > 50 or scale(write_rate, 1000000) > 50"
```

The function result has a regular type (string or float), so it slots into comparisons exactly like
a plain variable reference. The arguments themselves can be variables, literals, or — for nested
calls — other function calls: `%(format_bytes(scale(value, 1024)))` is valid.

### Variable-style shortcuts

Some checks expose pre-scaled "view variables" for the most common cases — `check_pdh` provides
`value_human`, `value_mb`, `value_gb`, etc. These are syntactic sugar for the corresponding function
calls:

```text
# Variable-style shortcut
check_pdh counter=mem "detail-syntax=Mem: %(value_human)"
check_pdh counter=mem "warning=value_mb > 500"

# Equivalent function calls
check_pdh counter=mem "detail-syntax=Mem: %(format_bytes(value))"
check_pdh counter=mem "warning=convert_bytes(value, 'MB') > 500"
```

Reach for variables when one of the prebuilt units fits; reach for functions when you need a custom
unit, a custom divisor, or composition with other expressions.

---

## 7. Performance Data

Performance data is the machine-readable metrics used for graphing, in the standard Nagios format:

```
'metric_name'=value[unit];[warn];[crit];[min];[max]
```

Example:

```
'total 5m'=2%;80;90 'total 1m'=5%;80;90 'total 5s'=11%;80;90
```

### Perfdata needs thresholds

Without `warn`/`crit`, perfdata values are emitted but empty:

```
check_cpu warning=none critical=none
'total 5m'= 'total 1m'= 'total 5s'=
```

To get values without alerts, use `perf-config` to mark metrics as "extra":

```
check_cpu warning=none critical=none "perf-config=extra(load)"
```

### Customising with `perf-config`

`perf-config` works like a tiny stylesheet — selectors target metrics, keys transform them.

```
"perf-config=selector(key:value; key:value) selector2(key:value)"
```

| Key                 | Effect                                                                     |
|---------------------|----------------------------------------------------------------------------|
| `unit`              | Force a unit (`G`, `M`, `K`, `%`, `ms`, …)                                 |
| `ignored`           | `true` → drop this metric                                                  |
| `prefix` / `suffix` | Rename parts of the metric name                                            |
| `minimum` / `min`   | Force the perfdata `min` field (use `min` as a shorthand)                  |
| `maximum` / `max`   | Force the perfdata `max` field — useful for graphing systems that auto-fit |

Selectors match in order of specificity: `prefix.object.suffix` → `prefix.object` → `object.suffix` → `prefix` →
`suffix` → `object`. The `*` selector matches everything.

#### Recipes

```
# Lock memory metrics to GB (avoids graph jumps when auto-scaling switches units)
check_memory "perf-config=*(unit:G)"

# Lock disk metrics to GB
check_drivesize "perf-config=*(unit:G)"

# Drop the percent metrics from check_drivesize, keep the absolute ones
check_drivesize "perf-config=used %(ignored:true)"

# Rename: drop suffix label, force GB
check_drivesize "perf-config=used.used(unit:G;suffix:'') used %(ignored:true)"
'C:\'=213G;178;201;0;223 'D:\'=400G;372;419;0;465

# Force min/max bounds on a counter that doesn't know its own range
# (e.g. a raw PDH counter exposed by check_pdh). The graphing system can
# then auto-scale to the declared range instead of guessing from history.
check_pdh "counter=\\Processor(_Total)\\% Processor Time" \
  "perf-config=*(minimum:0;maximum:100)"

# Same idea on a custom queue-depth counter, with `min`/`max` shorthand.
check_pdh counter=queue_depth "perf-config=queue_depth(min:0;max:12345)"
```

### Inspecting performance data

```
render_perf remove-perf command=check_drivesize
OK: OK:
C:\ used      213.605 GB      178.777 201.124 223.471 0
C:\ used %    95      %       79      89      100     0
```

### `perf-syntax` — naming metrics

`perf-syntax` controls the metric **name** (not its value), using the same template variables as `detail-syntax`:

```
check_cpu "perf-syntax=%(core) %(time)"     # 'total 5m'=...
check_cpu "perf-syntax=%(core)_%(time)"     # 'total_5m'=...
```

Useful when your graphing system is picky about names.

---

## 8. Putting It Together

Pick a check, run `show-default`, identify the option you want to change, change just that one. Repeat.

```
# Default behaviour
check_drivesize

# Step 1 — only fixed disks
check_drivesize "filter=type = 'fixed'"

# Step 2 — tighter thresholds
check_drivesize "filter=type = 'fixed'" \
  "warn=free_pct < 15" "crit=free_pct < 5"

# Step 3 — clean message with human-readable sizes (functions from section 6)
check_drivesize "filter=type = 'fixed'" \
  "warn=free_pct < 15" "crit=free_pct < 5" \
  "top-syntax=%(status): %(list)" \
  "detail-syntax=%(drive_or_id) %(format_bytes(free)) free of %(format_bytes(size))"

# Step 4 — graph-friendly perfdata
check_drivesize "filter=type = 'fixed'" \
  "warn=free_pct < 15" "crit=free_pct < 5" \
  "perf-config=*(unit:G) used %(ignored:true)"
```
