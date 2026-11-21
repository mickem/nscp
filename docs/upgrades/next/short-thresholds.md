---
icon: "🎚️"
modules: [filters]
action: none
---
**Checks take `-w` and `-c`, like other monitoring plugins.**
Every filter check now accepts its bounds in the short form most plugins use:
`check_cpu -w 80 -c 90`, `check_drivesize -w 20% -c 10%`,
`check_http url=… -w 2 -c 5`. Each check knows which keyword a bare number
bounds and on which side, so `check_cpu -w 80` is `warning=load > 80` and
`check_drivesize -w 20%` is `warning=free < 20%`; `help` on a command says what
its `-w` stands for. Ranges in the monitoring plugins format work too (`10:`,
`~:10`, `10:20`, `@10:20`), and anything that is not a number or a range is
used as the expression it is. A short threshold moves the number and leaves
the rest of the check's threshold in place, so `check_http -c 5` is still
critical on a failed request; every command lists the default of its `-w` and
`-c` next to the default `warning` and `critical` in `help` and
`show-default`. Durations are in seconds unless a unit is given
(`500ms`, `5m`), also for checks whose `time` keyword counts milliseconds. A
check that measures no single value bounds `count`, the number of matched
items: `check_process process=nginx -c 1:`. See
[Short thresholds](../concepts/checks.md#short-thresholds-w-and-c) for the
keyword of each check. Nothing to do: `warning=` and `critical=` are unchanged,
and a check that is given neither `-w` nor `-c` behaves as before. The only new
names are the options `w` and `c`, which no check used.
