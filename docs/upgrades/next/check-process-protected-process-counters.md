---
icon: "📊"
modules: [CheckSystem]
action: conditional
---
**`check_process` reads CPU time, handles and memory for protected processes,
and reports `unknown` for what it still cannot read.** Protected processes
(`csrss.exe`, `services.exe`, `MsMpEng.exe`, …) and, for an unprivileged
agent, other users' processes were listed with every counter at `0`, so
`delta=true "warn=time > 50"` or `"crit=working_set > 2G"` could never fire on
them. Their CPU times (including `delta=true`), creation time, handle counts
and, where Windows allows it, memory counters are now read. A counter that
still cannot be read renders `unknown`, never satisfies a threshold in either
direction and emits no performance data, instead of a `0` that looked like a
reading; test for it with `working_set = 'unknown'`. The same applies to all
counters (and `creation`) with `scan-info=false`, and to a `total=true` row
that sums such a process. Check your setup if you graph these processes'
perf data, or alert on a low value such as `handles < 50` or an old
`creation`: those series and alerts now go silent where the value is unknown
instead of reporting `0`. The new `limited` keyword tells these processes
apart. See
[`check_process`](../reference/check/CheckSystem.md#check_process).
