#### About `check_printqueue`

`check_printqueue` monitors Windows **print queues** — the classic "the print
server is stuck" incident. It reads `Win32_Printer` (status, error state and the
device inventory) and `Win32_PrintJob` (queued jobs), producing one row per
printer with its queue depth and the age of the oldest waiting job.

For the individual jobs behind those counts — who submitted what, how big it is
and how long it has been waiting — use [`check_printjobs`](#check_printjobs),
which reports one row per job.

Keywords (one row per printer):

| Keyword          | Description                                                                     |
|------------------|---------------------------------------------------------------------------------|
| `printer`        | Printer / queue name                                                            |
| `status`         | Printer status: `idle`, `printing`, `offline`, `stopped_printing`, `warmup`, …  |
| `error_state`    | Detected error: `no_error`, `no_paper`, `low_toner`, `jammed`, `door_open`, …    |
| `jobs`           | Number of queued print jobs                                                      |
| `error_jobs`     | Queued jobs in an error state                                                    |
| `oldest_job_age` | **Seconds** since the oldest queued job (`-1` if the queue is empty)             |
| `offline`        | `1` if the printer is offline                                                    |
| `error`          | `1` if the printer is in a real error state (paper/toner/door/jam/service)       |
| `driver`         | Print driver the queue uses                                                      |
| `port`           | Port it prints through (`IP_10.0.0.20`, `USB001`, `PORTPROMPT:`, …)              |
| `location`       | Location configured on the queue (empty when unset)                              |
| `share`          | Share name (empty when the queue is not shared)                                  |
| `server`         | Print server hosting the queue (empty for a local queue)                         |
| `default`        | `1` if this is the default printer                                               |
| `shared`         | `1` if the queue is shared                                                       |
| `network`        | `1` if the queue is a network rather than local printer                          |

The device columns are the inventory half of the check: they answer "is this
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
