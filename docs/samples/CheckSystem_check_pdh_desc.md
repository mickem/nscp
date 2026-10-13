#### About `check_pdh`

`check_pdh` reads Windows performance (PDH) counters and turns each one into a
filter record. It is the general-purpose escape hatch for anything the
purpose-built checks do not cover — if it shows up in Performance Monitor, this
can alert on it.

Counters are named with `counter=` (repeatable), and can also be passed
positionally. There are no default thresholds, so a bare call reports the values
and returns OK; a call with no counter at all is an error rather than an empty OK.

The alias `check_counter` is accepted for backwards compatibility.

##### Instantaneous versus averaged counters

Many PDH counters are *rates* and are meaningless from a single sample — a
single read of `\Processor(_Total)\% Processor Time` returns whatever the last
interval happened to be, or zero. `averages=true` takes two samples a second
apart and reports the difference, which is what you want for any `/sec` or `%`
counter.

For anything you check often, prefer the **configured collection** path instead:
add the counter under `[/settings/system/windows/counters]` so the background
collector samples it continuously, then reference it by its configured name.
That makes the check itself instant, and lets `time=` ask for an average over a
window (`time=5m`) rather than a one-second snapshot. `time=` may be repeated to
report several windows at once, in which case the default perf and detail syntax
automatically grow a `${time}` component so the series stay distinct.

##### Localized counter names

Counter names are localized, so `\Processor(_Total)\% Processor Time` does not
exist on a German or French Windows. `resolution=` decides how the name is
looked up:

- `auto` (default) — try the localized name, then the English API, then index
  expansion. This is what makes a single portable configuration work across
  language variants.
- `english` — force English names regardless of the system language.
- `index` — expand numeric counter indexes to their localized names, which is
  what `expand-index=true` does explicitly.

##### Instances, types and error handling

`instances=true` expands a wildcard instance (`\Process(*)\...`) into one record
per instance. `type=` picks the value format (`double`, `long`, `large`, default
`large`) and `flags=` passes PDH format flags (`nocap100`, `1000`, `noscale`) —
`nocap100` is the one you want for a counter that legitimately exceeds 100%,
such as multi-core `% Processor Time`.

A counter defined in `[/settings/system/windows/counters]` that does not exist
when NSClient++ starts - typically one a service registers when it starts, later
than NSClient++ - does not stop the others from being collected. The collector
keeps retrying it in the background: quickly during the first minute, then once
a minute. It logs `Failed to add counter <name> (<path>): ...` once the first
minute is over, and `Counter <name> is now available` once the counter is
collected. Until then, naming it in `counter=` fails with `Failed to get value`.
A counter that can never resolve - its object exists but has no counter by that
name, typically a typo - is logged once as `Ignoring counter <name> (<path>): ...`
and not retried.
`reload=true` asks the collector to retry the missing counters right away, and
waits up to 5 seconds for it, before the check reads a named counter that is not
there yet; it changes nothing for a counter given as a path.

`ignore-errors=true` makes a
missing or invalid counter report `0` instead of failing the check — convenient
for a configuration shared across differently-provisioned hosts, but be aware
that it turns "the counter is gone" into a silent zero, which is exactly the
kind of thing you may want to alert on.

Note that reading performance counters requires membership of the
**Performance Log Users** group (or equivalent rights); a check that returns
access-denied errors is usually a permissions problem, not a missing counter.
