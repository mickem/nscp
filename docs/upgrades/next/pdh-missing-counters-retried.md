---
icon: "⏱️"
modules: [CheckSystem]
action: conditional
---
**A missing predefined performance counter no longer disables the others, and
is picked up once it exists.** The counters in
`[/settings/system/windows/counters]` are now added one at a time: one that is
not registered (typically a counter a service registers when it starts, after
NSClient++ did) used to fail the whole collector query, so every predefined
counter answered `Failed to get value` until the next restart (#634). A missing
counter is now retried in the background, quickly during the first minute and
then every minute, for as long as it stays missing; when it turns up the log
says `Counter <name> is now available`. The error is still logged once, after
the first minute, and now reads
`Failed to add counter <name> (<path>): <reason> - retrying every 60 seconds`
instead of `Exception in Failed to add counter <name>:`. Update a log alert
that matches the old wording. At startup, `Not checking PDH data` is now only
logged when no counters are configured; with counters still pending it says how
many are being retried. `check_pdh reload=true` was accepted but ignored; it now
asks the collector to retry the missing predefined counters, and waits up to
5 seconds for that, before the check reads them.
