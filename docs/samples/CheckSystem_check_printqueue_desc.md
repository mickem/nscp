#### About `check_printqueue`

`check_printqueue` monitors Windows **print queues** — the classic "the print
server is stuck" incident. It reads `Win32_Printer` (status and error state) and
`Win32_PrintJob` (queued jobs), producing one row per printer with its queue
depth and the age of the oldest waiting job.

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

`oldest_job_age` is seconds, so threshold it with durations: `oldest_job_age > 30m`.

Defaults: **WARNING** when `jobs > 10`, **CRITICAL** when `error = 1`.
Offline printers are **not** alerted by default — virtual printers (Print to
PDF, OneNote) and disconnected USB printers are routinely offline — so opt in
with the `offline` keyword where it matters (e.g. a print server). empty-state is
**OK** (a host with no printers is fine).
