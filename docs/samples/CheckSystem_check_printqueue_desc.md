#### About `check_printqueue`

`check_printqueue` monitors Windows **print queues** — the classic "the print
server is stuck" incident. It reads `Win32_Printer` (status, error state and the
device inventory) and `Win32_PrintJob` (queued jobs), producing one row per
printer with its queue depth and the age of the oldest waiting job.

For the individual jobs behind those counts — who submitted what, how big it is
and how long it has been waiting — use [`check_printjobs`](#check_printjobs),
which reports one row per job.

The device keywords (`driver`, `port`, `location`, `share`, `server`, `default`,
`shared`, `network`) are the inventory half of the check: they answer "is this
queue still pointing at the driver and port it is supposed to", which is the
other common cause of "printing is broken" once the queue itself looks healthy.
They are also useful as a filter — `filter=shared = 1` to watch only what a
print server actually publishes.

`oldest_job_age` is seconds and takes durations: `oldest_job_age > 30m`,
`oldest_job_age > 2h`. A bare number still means seconds. An empty queue reports
`-1`, which is below every threshold, so it cannot raise a stuck-queue alert.

Defaults: **WARNING** when `jobs > 10`, **CRITICAL** when `error = 1`.
Offline printers are **not** alerted by default — virtual printers (Print to
PDF, OneNote) and disconnected USB printers are routinely offline — so opt in
with the `offline` keyword where it matters (e.g. a print server). empty-state is
**OK** (a host with no printers is fine).
