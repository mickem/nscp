---
icon: "🖥️"
modules: [CheckSystem]
action: none
---
**New check: `check_gpu` (Windows).** GPU load and GPU memory per adapter,
read from the vendor-neutral performance counter sets Windows maintains for
every WDDM adapter (`GPU Engine`, `GPU Adapter Memory`, `GPU Local Adapter
Memory`, `GPU Process Memory`) — no vendor tool or driver SDK needed, so VDI
hosts, RDS hosts with GPU offload and ML workstations all read the same way.
`utilization` is the load of the busiest engine (what Task Manager shows), with
per-type keywords for 3D, copy, video decode, video encode and compute; memory
is reported as `dedicated_used`/`dedicated_pct`, `shared_used`/`shared_pct`,
`committed` and `local_used`, and `processes` counts the processes holding
memory on the adapter. The defaults warn at `dedicated_pct > 85` and go
critical at `dedicated_pct > 95`. Needs Windows 10 1709 / Windows Server 2019
or later; a host without a real GPU returns `UNKNOWN: No GPU adapters found`
unless `empty-state=ok` is given, and a host where DXGI cannot list the
adapters returns `UNKNOWN: GPU adapters could not be identified through DXGI
...` rather than reporting them as healthy. Cards of the same model are told
apart by a ` #0`, ` #1`, ... suffix on `name`. On the 32-bit agent a card with
4 GB or more whose size the driver does not record has `dedicated_total` and
`dedicated_pct` of `0`, so the default thresholds do not apply to it. The check
is marked experimental.
