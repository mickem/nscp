#### About `check_gpu`

`check_gpu` reports GPU load and GPU memory, one record per adapter. It reads
the performance counter sets Windows itself maintains for every WDDM adapter —
the numbers behind Task Manager's GPU graphs — so it needs no vendor tool or
driver SDK and reads an NVIDIA, AMD or Intel adapter the same way.

| Counter set                | Used for                                             |
|----------------------------|------------------------------------------------------|
| `GPU Engine`               | `utilization` and the per-engine-type load keywords  |
| `GPU Adapter Memory`       | `dedicated_used`, `shared_used`, `committed`         |
| `GPU Local Adapter Memory` | `local_used`                                         |
| `GPU Process Memory`       | `processes`                                          |

The counter sets exist on **Windows 10 1709 / Windows Server 2019 and later**.
On anything older the check returns
`UNKNOWN: GPU performance counters are not available ...`.

##### What `utilization` means

A GPU is not one resource: it has several engines (3D, copy, video decode,
video encode, compute, ...) that run side by side. `utilization` is the load of
the **busiest engine**, which is the figure Task Manager shows for the adapter;
`busiest_engine` names it. The per-type keywords (`utilization_3d`,
`utilization_video_decode`, `utilization_compute`, ...) let a check watch one
kind of work, for example the encoder on a session host.

The load is sampled over **one second**, so the check takes a second to run and
the figure is a snapshot. A GPU pinned at 100% is also the normal state of a
render or training machine. For both reasons the default thresholds leave the
load alone and watch memory instead: `dedicated_pct > 85` warns and
`dedicated_pct > 95` is critical. Running out of dedicated memory is what
pushes work to shared memory (system RAM) and makes a session host stutter.
When you do threshold `utilization`, let the monitoring server confirm it over
several check attempts rather than alerting on a single sample.

##### Names and memory sizes

The counters carry usage only. The adapter's name and its memory sizes
(`dedicated_total`, `shared_total`, and with them `dedicated_pct` and
`shared_pct`) are looked up through DXGI and matched on the adapter's `luid`.
An adapter that cannot be identified that way is still reported: under its
`luid`, with `adapter_type = 'unknown'`, totals of `0` and percentages of `0`.
Threshold on the byte keywords (`dedicated_used > 6G`) where that matters.

The `luid` is assigned when the driver starts and **changes on reboot**; select
an adapter by `name`, not by `luid`.

##### Hosts without a GPU

Every Windows host carries the *Microsoft Basic Render Driver*, a software
rasteriser. It is reported with `adapter_type = 'software'` and the default
filter (`adapter_type != 'software'`) drops it, so a host without a real GPU —
most virtual machines — ends up with nothing to report and returns
**`UNKNOWN: No GPU adapters found`**. Use `empty-state=ok` when the same check
definition is deployed to hosts with and without a GPU, and `filter=none` to
see every adapter Windows knows about.

##### What it does not cover

There is no per-process view: `processes` counts the processes holding memory
on an adapter but does not name them. Temperature, fan speed and power draw are
not in the Windows counter sets and still need the vendor's own tool.
