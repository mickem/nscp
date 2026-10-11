---
icon: "📊"
modules: [CheckSystem]
action: conditional
---
**PDH checks retry a briefly missing counter instance, and `check_pdh` errors name the counter.**
A counter whose instance does not exist at the moment of the sample (typically
a `\PhysicalDisk(...)` instance whose name changed, or a RAID volume that
dropped out for a moment) made `check_pdh` go UNKNOWN straight away with
`Failed to add counter: PdhCollectQueryData failed: : 800007d5: ...` (#648).
Now:

- `check_pdh` (also with `averages=true`) takes one more sample a second
  later before reporting a missing instance. The state is still UNKNOWN when it
  stays missing. The background collector behind `check_cpu` and named
  counters does not retry; its next tick is the next sample.
- `check_pdh` reports a failure after the counters were added as
  `Failed to poll counter: No data for <counter path> (does the instance exist
  right now?): PDH 0x800007D5: ...`, and a failure to open the query or add a
  counter to it as `Failed to open counter query: ...`. A counter path that
  cannot be resolved at all still reports `Failed to add counter`.
- `ignore-errors=true` now does what it says for `check_pdh`: a counter with no
  value this tick is skipped instead of failing the check, and the missing
  instance is not retried. Before, only `instances=true` skipped failing
  counters, and `ignore-errors` only hid the failure of the whole check.

Check your setup only if you alert on, or filter, the text
`Failed to add counter`: match `Failed to poll counter` and
`Failed to open counter query` as well.
