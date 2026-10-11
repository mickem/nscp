---
icon: "📊"
modules: [CheckSystem]
action: conditional
---
**`check_pdh`: a counter whose instance is briefly missing is retried, and the error names it.**
When PDH returned no data for any counter in a `check_pdh` query (`0x800007D5`,
typically a `\PhysicalDisk(...)` instance whose name changed or a RAID volume
that dropped out for a moment), the check went UNKNOWN straight away with
`Failed to add counter: PdhCollectQueryData failed: : ...`. It now takes one
more sample a second later before reporting, and a failure after the counters
were added reads `Failed to poll counter: No data for <counter path> (does the
instance exist right now?): PDH 0x800007D5: ...` (#648). The state is still
UNKNOWN when the instance stays missing. Only if you alert on, or filter, the
text `Failed to add counter` for these failures: match `Failed to poll counter`
as well. A counter that genuinely cannot be added still reports
`Failed to add counter`.
