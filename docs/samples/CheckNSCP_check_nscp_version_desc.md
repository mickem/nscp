#### About `check_nscp_version`

`check_nscp_version` reports the version of the running NSClient++ build. It
decomposes the version so that thresholds can be written against it rather than
against a string: `release`, `major`, `minor` and `build` are numeric, while
`version` and `date` carry the rendered string and the build date.

The typical use is fleet hygiene — flag agents that have fallen behind a version
you have standardised on:

```
check_nscp_version "crit=major < 12"
```

There are no default thresholds, so a bare call is always OK and simply reports
the version. That makes it a useful heartbeat probe as well: the cheapest way to
confirm an agent is up and answering.

Note that `build` is only meaningful on builds before 0.6.0; newer releases
report `0` for it.

If you want to know whether a *newer* release exists rather than which one is
installed, use [`check_nscp_update`](#check_nscp_update), which compares against
the latest release published on GitHub. For the running agent's own health —
crashes, logged errors, uptime — see [`check_nscp`](#check_nscp).
