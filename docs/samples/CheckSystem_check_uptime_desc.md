#### About `check_uptime`

`check_uptime` reports how long the machine has been running since its last
boot, as a single aggregate row.

The defaults invert the usual reading of "uptime": the check warns when uptime
is **less** than 2 days and goes critical below 1 day. That is deliberate — a
low uptime means the machine has just rebooted, which is the event worth
alerting on. A high uptime is only a problem if your patching policy makes it
one, in which case invert the comparison:

```
check_uptime "warn=uptime > 90d" "crit=uptime > 180d"
```

`uptime` accepts units, so thresholds are written the way you think about them
(`2d`, `12h`, `90d`) rather than in raw seconds.

##### Rendering the duration

`max-unit=` controls the largest unit `${uptime}` is rendered in — `s`, `m`,
`h`, `d` or `w`, defaulting to `w`. For a six-week uptime, `w` renders
`6w 0d 00:00`, `d` renders `42d 00:00` and `h` renders `1008:00`. Pick whichever
reads best for the audience; it affects only the rendered string, never the
comparisons.

##### Boot time and timezone

`boot` is the wall-clock time the machine came up, derived as *now minus
uptime*, and `${tz}` renders the timezone label it is expressed in. Both follow
the module's configured timezone (default `local`), so the boot time in the
message matches the clock an operator is reading it against.

The same duration formatting and unit handling is shared with `check_nscp`'s
`uptime` and `crash_age` keywords, so thresholds written for one read the same
way in the other.
