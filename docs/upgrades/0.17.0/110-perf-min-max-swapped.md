---
icon: "📊"
modules: [CheckHelpers]
---
**`filter_perf`/`render_perf`/`xform_perf`: the `max` and `min` filter
keywords were swapped.** `max` read the perf-data *minimum* bound and
`min` the *maximum*. They now read the bounds they name — a filter that
compensated for the swap needs the two names exchanged back.
