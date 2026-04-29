# Performance Data

Performance data is the machine-readable metric output that monitoring tools use for graphing and trend analysis. It is generated automatically when you specify `warn` and `crit` thresholds, and can be customised extensively.

---

## Performance Data Format

The format follows the Nagios plugin standard:

```
'metric_name'=value[unit];[warn];[crit];[min];[max]
```

Example from `check_cpu`:

```
'total 5m'=2%;80;90 'total 1m'=5%;80;90 'total 5s'=11%;80;90
```

- `'total 5m'` — metric name
- `2%` — current value (percent)
- `80` — warning threshold
- `90` — critical threshold

---

## Performance Data Requires Thresholds

Performance data is only generated when `warn` and `crit` are set. Without them, performance data is empty:

```
check_cpu warning=none critical=none
'total 5m'= 'total 1m'= 'total 5s'=
```

To get performance data without triggering alerts, add thresholds that will never be reached, or use `perf-config` with extra metrics:

```
check_cpu warning=none critical=none "perf-config=extra(load)"
'total 5m'=0%;0;0 'total 1m'=8%;0;0 'total 5s'=21%;0;0
```

---

## Customising Performance Data with `perf-config`

The `perf-config` option lets you change how performance data is formatted. It works like a mini CSS stylesheet: you select metrics by name and apply key-value transformations.

### Syntax

```
"perf-config=selector(key:value; key:value) selector2(key:value)"
```

### Available keys

| Key | Values | Effect |
|---|---|---|
| `unit` | Letter (`G`, `M`, `K`, `%`, `ms`, …) | Force a specific unit |
| `ignored` | `true` or `false` | Exclude this metric from output |
| `prefix` | String | Change the metric name prefix |
| `suffix` | String | Change the metric name suffix |

### Selectors

Selectors match metric names in this order (most specific first):

1. `prefix.object.suffix`
2. `prefix.object`
3. `object.suffix`
4. `prefix`
5. `suffix`
6. `object`

---

## Common `perf-config` Recipes

### Lock memory metrics to gigabytes (prevents auto-scaling)

Auto-scaling can confuse graphing systems (e.g., a value that shifts from `800M` to `1.2G` between checks):

```
check_memory "perf-config=*(unit:G)"
```

The `*` selector matches all metrics.

### Lock disk metrics to gigabytes

```
check_drivesize "perf-config=*(unit:G)"
```

### Remove percentage metrics and keep only absolute values

For `check_drivesize`, metrics come in pairs: `C:\ used` (GB) and `C:\ used %` (percent).

To drop the percentage and keep only GB:

```
check_drivesize "perf-config=used %(ignored:true)"
```

### Change unit and remove the suffix label

```
check_drivesize "perf-config=used.used(unit:G;suffix:'') used %(ignored:true)"
```

Output (metric name becomes just the drive letter):

```
'C:\'=213G;178;201;0;223 'D:\'=400G;372;419;0;465
```

!!! note
    The `used.used(...)` selector uses the dot notation `object.suffix` to target only metrics named `used` whose suffix is also `used` — this avoids accidentally matching `used %`.

---

## Viewing Formatted Performance Data

To inspect performance data in a table format, use the `render_perf` helper:

```
render_perf remove-perf command=check_drivesize
OK: OK:
C:\ used      213.605 GB      178.777 201.124 223.471 0
C:\ used %    95      %       79      89      100     0
D:\ used      400.713 GB      372.607 419.183 465.759 0
```

---

## The `perf-syntax` Keyword

The `perf-syntax` option controls what the metric *name* looks like (not the value). It supports the same template variables as `detail-syntax`:

```
check_cpu "perf-syntax=${core} ${time}"
'total 5m'=2%;80;90 ...
```

```
check_cpu "perf-syntax=${core}_${time}"
'total_5m'=2%;80;90 ...
```

This can help if your graphing system has specific naming requirements.

---

## Next Steps

- [Filters and Expressions](filters.md) — control which items are included
- [Thresholds](thresholds.md) — control which items trigger alerts
- [Output Syntax](syntax.md) — control the message text
- [Monitoring Scenarios](../scenarios/index.md) — see all these concepts in real-world examples
