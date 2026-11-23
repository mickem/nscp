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

Drivers name their engines freely, so an engine counts toward a per-type
keyword by what its type contains (case, spaces, underscores and a trailing
engine number ignored):

| Engine type contains | Counts toward                                             | Examples                                     |
|----------------------|-----------------------------------------------------------|----------------------------------------------|
| `3D`                 | `utilization_3d`                                          | `3D`, `High Priority 3D`                     |
| `Copy`               | `utilization_copy`                                        | `Copy`                                       |
| `VideoDecode`        | `utilization_video_decode`                                | `VideoDecode`, `Video Decode`                |
| `VideoEncode`        | `utilization_video_encode`                                | `VideoEncode`                                |
| `VideoCodec`         | `utilization_video_decode` and `utilization_video_encode` | `Video Codec 0` (one engine does both)       |
| `Compute` or `Cuda`  | `utilization_compute`                                     | `Compute_0`, `High Priority Compute`, `Cuda` |

Any other engine — `VideoProcessing` (scaling, deinterlacing and colour
conversion, which is neither decoding nor encoding), `Security`,
`LegacyOverlay`, ... — counts toward none of them, but still toward
`utilization` and `busiest_engine`. So `utilization` can be high while every
per-type keyword is low; `busiest_engine` says which engine it is.

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

If DXGI itself fails (it cannot be loaded, or listing or describing an adapter
fails), the check does not guess: no adapter could be told apart from the
software rasteriser and no percentage could be computed, so it returns
`UNKNOWN: GPU adapters could not be identified through DXGI, so their names,
types and memory sizes are unknown (...)` with the failing call and its error
code in the brackets. `empty-state=ok` does not turn that into an OK.

The **32-bit (x86) agent** gets memory sizes from DXGI as 32-bit numbers, which
cannot describe a card with 4 GB or more. There it takes the size of such a
card from the driver's `HardwareInformation.qwMemorySize` registry value when
exactly one size is recorded for that PCI device; failing that, a size at or
above 3.75 GB is treated as unknown — `dedicated_total` (or `shared_total`) is
`0` and so is its percentage, so the default thresholds do not fire for that
adapter. Use the 64-bit agent, or threshold on `dedicated_used`, on such hosts.

Two adapters never share a `name`: when several would — two cards of the same
model, or the physical adapters of a linked adapter — each gets ` #0`, ` #1`,
... appended, in DXGI enumeration order (which follows the PCI slots and
survives a reboot) and then by `phys`. A card whose name nothing else shares is
reported without a suffix. The suffix keeps perfdata labels (`perf-syntax` is
`${name}`) apart and lets `filter=name = 'NVIDIA A16 #1'` pick one card.

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
