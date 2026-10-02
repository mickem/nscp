---
icon: "⏱️"
modules: [CheckSystem, CheckSystemUnix]
action: none
---
**Collector-backed checks can choose the status they report while warming up.**
Nothing to do. Right after the agent or the module starts, the background
collector has not taken its first sample yet, and `check_cpu`, `check_load`
(Windows) and, on Linux and macOS, `check_memory`, `check_pagefile` and
`check_network` answer UNKNOWN *"… not available yet (collector still
initializing)"*. A new `warmup-state` option (`ok`, `warning`, `critical` or
`unknown`, default `unknown`) picks that status instead, for instance
`warmup-state=ok` to keep a restart from raising alerts; the message still says
the collector is initializing. It only ever covers a collector that has not
tried yet: one that is disabled, busy or failing stays UNKNOWN whatever the
option says (see
[Collector-backed checks and warm-up](../concepts/checks.md#8-collector-backed-checks-and-warm-up)).

Telling those apart changes a few results even without the option. Each is
still UNKNOWN unless noted, only with a message that says what is wrong:

- **Linux and macOS** - a source the collector cannot read (an empty or
  unreadable `/proc/stat`, `/proc/meminfo` or `/proc/net/dev` in a locked-down
  container) used to be stored as an all-zero sample, so `check_cpu` reported
  0% load and `check_memory` a zero total as if measured. `check_cpu`,
  `check_memory`, `check_pagefile` and `check_network` now answer UNKNOWN
  *"the collector failed to sample it"* with the reason, and each source is
  judged on its own, so an unreadable `/proc/stat` does not affect
  `check_memory`.
- **Windows `check_load`** - `disable = load` now reports *"Load average
  sampling is disabled"*, and a collector whose lock could not be had reports
  *"the collector is busy"*; both used to share the warm-up message.
- **Windows `check_cpu`** - a tick that failed to read the CPU load is no longer
  stored as an idle sample, so it cannot pull the averages down, and
  `disable = metrics` no longer stops the CPU samples (`check_cpu` used to
  report *"collector still initializing"* for good with it set). With
  `use pdh for cpu` enabled, an explicit `warmup-state` is refused, as that
  source has no warm-up to report.
- **Both** - an invalid `time=` on `check_cpu` is reported right away instead of
  after the warm-up.
