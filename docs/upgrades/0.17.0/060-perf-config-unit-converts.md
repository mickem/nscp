---
icon: "📊"
modules: [filters]
action: conditional
---
**`perf-config`'s `unit:` now converts plain byte series instead of
relabelling them.** On series that are byte counts but do not auto-scale
(most byte keywords outside `check_drivesize`), `unit:KB` used to change the
label only, shipping `=1536KB` for a value of 1536 *bytes* - a metric off by
the unit ratio to any consumer that trusts the label. The value and the
warn/crit bounds now convert into the requested unit, matching what the
auto-scaling series always did. A dashboard that compensated for the
mislabelling will see the metric drop by that ratio on upgrade. Series not
measured in bytes (`ms`, `%`, `s`, ...) and explicit `minimum:`/`maximum:`
overrides are unaffected, and a `unit:` that names no byte unit still only
changes the label.
