**Default check:**

```
check_uptime
uptime: -9:02, boot: 2013-aug-18 08:29:13 (local)
'uptime uptime'=1376814553s;1376760683;1376803883
```

Adding **warning and critical thresholds**::

```
check_uptime "warn=uptime < -2d" "crit=uptime < -1d"
...
```

Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_uptime
uptime: -0:3, boot: 2013-sep-08 18:41:06 (local)|'uptime'=1378665666;1378579481;1378622681
```

**Configuring the timezone** (added in 0.6.x). The default syntax renders
the boot timestamp in the configured zone and surfaces a short label via
the `${tz}` placeholder. The value is cached by each plugin in its
`loadModuleEx` and is read from the global `/settings/default/timezone`
setting. Accepted values: `local` (default), `utc`, or any POSIX TZ string
parseable by Boost.Date_time (for example `MST-07` or
`EST-05EDT,M3.2.0,M11.1.0`).

**Choosing the display granularity for `${uptime}`** (issue #590). The
`max-unit` argument selects the largest unit allowed when rendering
`${uptime}`. Accepted values: `s|m|h|d|w` (default `w`). For example, on
a host that has been up six weeks, `max-unit=w` renders `6w 0d 00:00`,
`max-unit=d` renders `42d 00:00`, and `max-unit=h` renders `1008:00`:

```
check_uptime max-unit=d "detail-syntax=uptime: ${uptime}, boot: ${boot} (${tz})"
```

