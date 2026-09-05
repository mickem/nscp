---
icon: "📊"
modules: [filters]
action: conditional
---
**A `unit:` in `perf-config` that names no unit no longer divides the metric
by 1024⁷.** An unrecognised unit now leaves the value alone. If a graph of
yours has been flat at a near-zero value, check the `unit:` spelling in its
`perf-config`: the metric will jump to its real magnitude on upgrade.
