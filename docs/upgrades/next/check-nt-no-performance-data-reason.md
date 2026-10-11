---
icon: "🔧"
modules: [NSClientServer]
action: none
---
**check_nt's "No performance data" error now says why.** When a check behind a
check_nt request (`CPULOAD`, `UPTIME`, `MEMUSE`, `USEDDISKSPACE`, `COUNTER`,
`FILEAGE`) answers without a value, the reply used to be just
`ERROR: No performance data from command: check_cpu`. It now carries the
check's own explanation, e.g. `ERROR: No performance data from command:
check_cpu: Failed to read CPU data: the collector is busy (timed out waiting for
its lock)`. A monitoring rule that matches the old text exactly needs to match
the prefix instead.
