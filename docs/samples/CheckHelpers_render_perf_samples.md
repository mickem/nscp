**Render a check's performance data as the message:**

The result carries two lines: the wrapped check's own result, then the rendered
one. The default `detail-syntax` produces one tab-separated row per counter —
key, value, unit, warning, critical, min, max.

```
render_perf command=check_drivesize
OK: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.24544GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100
OK: OK:  / used	8.24544	GB	201.578	226.775	0	251.972	...
```

**Drop the original performance data (`remove-perf=true`):**

Useful when the rendered message is the point and you do not want every number
duplicated as a graphed series.

```
render_perf command=check_drivesize remove-perf=true
OK: WARNING /opt/claude-code: 202.746MB/229.949MB used
OK: OK:  / used	8.24618	GB	201.578	226.775	0	251.972	...
```

**Pick out just the counters you care about:**

`like` is substring matching, so this keeps only the percentage counters.

```
render_perf command=check_drivesize "filter=key like '%'" "detail-syntax=${key}=${value}${unit}"
OK: WARNING /opt/claude-code: 202.746MB/229.949MB used
OK: OK:  / used %=3%, /opt/claude-code used %=88%, /opt/env-runner used %=64%
```

**Threshold on a value the wrapped check does not expose in its message:**

Note that the rendered line lists *every* matching counter, not only the ones
that breached — the default `top-syntax` is `%(status): %(message) %(list)`.

```
render_perf command=check_drivesize "filter=key like 'used %'" "crit=value > 80" "detail-syntax=${key}=${value}%"
CRITICAL: WARNING /opt/claude-code: 202.746MB/229.949MB used
CRITICAL: CRITICAL:  / used %=3%, /opt/claude-code used %=88%, /opt/env-runner used %=64%
```

The wrapped check itself was only WARNING here; the CRITICAL comes from
`render_perf`'s own threshold on the counter value.

**A check with no performance data reports UNKNOWN:**

`empty-state` defaults to `unknown`, so an empty result is distinguishable from
a healthy one rather than reading as OK.

```
render_perf command=check_ok
UNKNOWN: No message
UNKNOWN: UNKNOWN:
```
