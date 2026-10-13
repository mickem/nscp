---
icon: "⏱️"
modules: [CheckSystem]
action: conditional
---
**A missing predefined performance counter no longer disables the others, and
is picked up once it exists.** One counter in
`[/settings/system/windows/counters]` that was not registered (typically a
counter a service registers when it starts, after NSClient++ did) used to fail
the whole collector query, so every predefined counter answered
`Failed to get value` until the next restart (#634). Now:

- Each counter is added on its own, and a missing one is retried in the
  background: quickly during the first minute, then every minute.
- A counter still missing after the first minute is logged once as
  `Failed to add counter <name> (<path>): <reason> - retrying every 60 seconds`
  (it used to be `Exception in Failed to add counter <name>:`); when it turns
  up the log says `Counter <name> is now available`. Update a log alert that
  matches the old wording.
- A predefined counter is visible to checks once the collector has sampled
  it. A check in the first seconds after startup that used to read `0` reports
  `Failed to get value` instead.
- `check_pdh reload=true` was accepted but ignored. It now has the collector
  retry the missing counters on its next tick; the check itself does not wait.
- With `disable = metrics` the predefined counters are still sampled (they
  used to read `0` forever). With `disable = pdh` they are not loaded, and
  `check_pdh` reports `Failed to get value` for them instead of `0`.
- `Not checking PDH data` is only logged at startup when no counters are
  configured; otherwise the collector says how many are still being retried.
