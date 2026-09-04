---
icon: "💥"
modules: [CheckNSCP]
---
**`check_nscp` is now a filter check, and it can see crash reports again.**
Crash reporting itself has always worked — the agent has archived crash
dumps since 0.4.x — but `check_nscp`'s *count* of them has not, for two
independent reasons that accumulated over the years:

- The count matched files whose extension equalled `txt`, while the helper
  it used returns the extension *with* its leading dot (`.txt`). That
  comparison has been false since the check was written in 0.4.2, so the
  crash count read 0 whatever was in the folder.
- 0.6.10 dropped Google Breakpad, whose vendored submodule and build
  machinery had become a dependency burden, along with the separate
  crash-report sender tool that shipped with it. The handler that replaced
  it writes one plain-text `<timestamp>.crash` file per crash — the
  exception, the faulting address and the module it landed in — where
  breakpad left a `<guid>.dmp` minidump plus a `<guid>.dmp.txt`
  description. From 0.6.10 on, even a corrected `.txt` match would have
  found nothing.

Separately, 0.4.3 stopped the module reading the archive folder from
`[/settings/crash]` `archive folder` and hardcoded the compile-time default
instead, so on any installation that had moved the archive folder the check
was looking in the wrong place as well.

All of that is fixed. Crash reports are recognised by `.crash`, and still by
`.dmp` and `.txt` so pre-0.6.10 archives keep counting; the configured
`archive folder` is read again; and `check_nscp` now exposes `crashes`,
`last_crash`, `crash_age`, `errors`, `last_error`, `uptime`, `version` and
`date` as filter keywords, accepts the usual `filter` / `warning` /
`critical` and `top-syntax` / `detail-syntax` options, and emits perfdata for
whichever keywords the thresholds name.

The default verdict is unchanged — any crash report or any logged error is
CRITICAL — but two things change for existing users. The message is now
`N crash(es), M error(s), uptime <duration>` without the appended
`last crash:` / `last error:` fragments. To put them back, pass a
`detail-syntax`:

```
check_nscp "detail-syntax=${crashes} crash(es) (${last_crash}), ${errors} error(s) (${last_error}), uptime ${uptime}"
```

And because the crash count now works, an agent with an old report still
sitting in the crash archive folder will start reporting CRITICAL where it
previously read 0 — threshold on `crash_age` (for example
`"crit=crash_age < 7d"`) if you only care about recent crashes, or clean the
folder out. Crash reports remain a Windows-only concept; on Linux there is no
crash handler, so `crashes` is always 0.
