---
icon: "⏱️"
modules: [CheckSystem, CheckSystemUnix]
action: none
---
**Collector-backed checks can choose the status they report while warming up.**
Nothing to do. Right after the agent or the module starts, the background
collector has not taken its first sample yet, and `check_cpu`, `check_load`
(Windows) and, on Linux, `check_memory`, `check_pagefile` and `check_network`
answer UNKNOWN *"… not available yet (collector still initializing)"*. A new
`warmup-state` option (`ok`, `warning`, `critical` or `unknown`, default
`unknown`) picks that status instead, for instance `warmup-state=ok` to keep a
restart from raising alerts; the message still says the collector is
initializing. Without the option every check answers exactly as before. On
Windows, `check_load` with `disable = load` now says *"Load average sampling is
disabled"* instead of the warm-up message it shared until now, and stays
UNKNOWN whatever `warmup-state` says.
