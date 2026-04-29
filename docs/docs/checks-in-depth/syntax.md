# Output Syntax

The output syntax controls what the status message looks like — the human-readable text that appears alongside OK/WARNING/CRITICAL in your monitoring tool. It has no effect on the check result itself.

---

## The Three Syntax Options

Every check supports three syntax keywords:

| Keyword | When it applies | Default purpose |
|---|---|---|
| `top-syntax` | Always — the overall message | Shows the status and list of problems |
| `detail-syntax` | Per item — how each item is formatted within the list | Shows per-item values |
| `ok-syntax` | When status is OK | Shows a brief "all ok" message |

### Defaults for `check_cpu`:

```
top-syntax=${status}: ${problem_list}
ok-syntax=%(status): CPU load is ok.
detail-syntax=${time}: ${load}%
```

---

## Template Variables

Syntax strings are text templates with keywords surrounded by `${}` or `%()`:

```
top-syntax=${status}: ${problem_list}
```

Both forms are equivalent from NSClient++'s perspective:

```
top-syntax=${problem_list}
top-syntax=%(problem_list)
```

!!! note
    On Unix shells (used to run Nagios commands), `${}` may be interpreted by the shell. Use `%()` when passing syntax strings through NRPE/Nagios to avoid escaping issues.

### Common template variables (all checks)

| Variable | Meaning |
|---|---|
| `${status}` / `%(status)` | Current status: `OK`, `WARNING`, `CRITICAL`, `UNKNOWN` |
| `${list}` / `%(list)` | All items that passed the filter, formatted by `detail-syntax` |
| `${problem_list}` / `%(problem_list)` | Only items that matched `warn` or `crit` |
| `${ok_list}` | Only items in OK state |
| `${warn_list}` | Only items in WARNING state |
| `${crit_list}` | Only items in CRITICAL state |
| `${count}` | Number of items matching the filter |
| `${problem_count}` | Number of warning or critical items |

The `detail-syntax` variables depend on the specific check. Use `check_cpu help` (or any check's `help` option) to see the full list for that check.

---

## Practical Examples

### Show the status and a list of all items (not just problems)

```
check_cpu "top-syntax=%(status): %(list)"
OK: 5m: 2%, 1m: 5%, 5s: 11%
```

### Show all values including OK (the `show-all` shortcut)

The `show-all` option replaces `%(problem_list)` with `%(list)` in the top-syntax automatically:

```
check_cpu show-all
OK: 5m: 2%, 1m: 5%, 5s: 11%
```

This is equivalent to:

```
check_cpu "top-syntax=%(status): %(list)"
```

### Custom CPU message

```
check_cpu "top-syntax=%(status): Cpu usage is %(list)" time=5m "detail-syntax=%(load)%"
OK: Cpu usage is 26%
```

### Custom memory message showing free and used

```
check_memory "top-syntax=${list}" "detail-syntax=${type} free: ${free} used: ${used} size: ${size}"
page free: 16G used: 7.98G size: 24G, physical free: 4.18G used: 7.8G size: 12G
```

### Service: show each service name and state

```
check_service "top-syntax=${list}" "detail-syntax=${name}: ${state}"
AdobeARMservice: running, Spooler: running, wuauserv: stopped
```

### Event log: show count in the message

```
check_eventlog "top-syntax=${status}: ${count} event(s) found — ${detail_list}"
CRITICAL: 3 event(s) found — Application: Error (Event 100), System: Error (Event 7036)
```

---

## The `empty-state` Option

When the filter matches no items (e.g., there are no stopped auto-start services), the `empty-state` option controls what status to return:

| Value | Meaning |
|---|---|
| `ok` | Return OK (default for most checks) |
| `warning` | Return WARNING |
| `critical` | Return CRITICAL |
| `ignored` | Suppress result entirely |

```
check_service "filter=name = 'NonExistentService'" empty-state=ok
OK: All services are ok.
```

---

## Next: Performance Data

Continue to [Performance Data](performance-data.md) →
