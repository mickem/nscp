#### How CPU load is measured (historical buffer)

`check_cpu` does not measure the CPU load at the moment the check is executed. Instead, NSClient++
runs a background collector thread that samples the CPU load roughly **once per second** and pushes
each sample into an in-memory ring buffer. Whenever you run `check_cpu` the values reported are
**averages computed from this buffer** for one or more time windows.

The time windows are controlled by the `time=` option. The default is to compute three averages:
`5m`, `1m` and `5s` (which is why the default output contains rows like `total 5m load`,
`total 1m load` and `total 5s load`). You can override this with one or more `time=` arguments,
for example `time=10m` or `time=30s time=2m`.

**Where the numbers come from**

Each sample is the share of time the CPU spent busy since the previous sample. It is computed from
the operating system's cumulative CPU time counters, not from a performance counter:

* **Windows:** the total comes from `GetSystemTimes()` and the per-core values from
  `NtQuerySystemInformation(SystemProcessorPerformanceInformation)`. Windows counts idle time
  inside kernel time, so idle is subtracted from it; what is left is the `system` keyword.
* **Linux:** the `cpu` and `cpuN` lines of `/proc/stat`. `idle` and `iowait` count as idle. Busy
  time is `user`, `nice`, `system`, `irq`, `softirq` and `steal`, so a VM that loses time to its
  host shows that time as load.
* **macOS:** `host_processor_info(PROCESSOR_CPU_LOAD_INFO)`.

On Windows the very first sample after the agent starts has no previous sample to compare with, so
it holds the average load since the machine booted, and it goes into the buffer like any other.
Right after a restart, while the windows still hold only a few samples, it pulls the short averages
(`5s`, and less so `1m`) towards the host's long-term average. It drops out once the window has
moved past it. Linux and macOS use their first reading only as the starting point and store no
sample for it.

`check_cpu` does not read the `\Processor(_Total)\% Processor Time` PDH counter. It measures the
same thing, so it tracks that counter in PerfMon closely, but the two will not match exactly:
they sample at different moments and `check_cpu` reports an average. Task Manager (since
Windows 8) shows a different counter, `% Processor Utility`, which is scaled by the current CPU
frequency. On a laptop or a server with turbo boost, it can differ from `check_cpu` by tens of
percent under steady load.

If you want the PDH counter itself, read it with `check_pdh` and the `averages` option:

```
check_pdh counter='\Processor(_Total)\% Processor Time' averages
```

`% Processor Time` is a rate, so a single sample of it is meaningless; `averages` makes the check
take two samples one second apart. The single quotes stop the `nscp test` prompt from treating the
backslashes as escapes. For an average over a longer window, configure the counter with
`collection strategy=rrd` and use `time=` (see the `check_pdh` samples).

**Buffer size and configuration**

The size of the historical buffer is controlled by the `default buffer length` setting on the
CheckSystem section. The default is `1h`, meaning the last hour of samples is retained. The buffer
size puts an upper bound on the time windows you can use:

* If you ask for a window that is **shorter than or equal to** the buffer length, the result is the
  average of all samples collected during that window.
* If you ask for a window that is **longer than** the buffer length, the result will only cover the
  samples that are actually present in the buffer (effectively capped to the buffer length).
* If NSClient++ was started **less time ago than the requested window**, the result will only
  reflect the samples collected since startup. Right after start-up `5m` and `1m` averages will
  therefore be based on fewer samples than they normally would be.
* Before the collector has taken its **first** sample (the first second or so after the agent
  starts) there is nothing to average, and the check answers
  `No CPU data available yet (collector still initializing)`. That is UNKNOWN by default; set
  `warmup-state=ok` (or `warning`/`critical`) to report a different status meanwhile. Only that
  first-sample case is affected: a window that is merely not full yet answers from what it has.

**One-shot `nscp client` cannot measure CPU load**

The buffer lives in the running process, so it only has history in an agent that keeps running.
`nscp client --module CheckSystem --boot --query check_cpu` starts a new process, loads the module,
asks at once and exits. The collector has not sampled yet, so the answer is always the warm-up one,
whatever `time=` says. This is also what you get from wrappers that run `nscp.exe client` once per
check, such as the Icinga 2 `nscp-local-cpu` check command or a scheduled task. Results from
`nscp test` look different because it keeps the agent running.

Run `check_cpu` against the agent that is already running instead: over NRPE, over the REST API
(for example with `check_nscp_api`), or from the agent's own scheduler with NSCA/NRDP.
`warmup-state=ok` only turns the warm-up answer green; it does not produce a reading.

If you need to check on longer windows (for example `2h` or `6h`) you must increase
`default buffer length` accordingly. Note that a larger buffer uses more memory, so only increase
it as far as you actually need.

**Impact on measurements**

Because every value reported by `check_cpu` is an average over a time window, the choice of `time=`
has a direct impact on what the check sees:

* **Short windows** (e.g. `5s`, `10s`) are very reactive and will show short spikes in CPU load,
  but they also produce a lot of noise. They are useful for catching transient bursts but can also
  generate flapping alerts.
* **Medium windows** (e.g. `1m`, `5m`) are a good compromise for most monitoring use cases. They
  smooth out short spikes while still reacting to sustained load within a few minutes.
* **Long windows** (e.g. `15m`, `1h`) smooth out almost all transients and only fire when the CPU
  has been busy for an extended period of time. They are well suited to detecting sustained load
  but will be slow to react and slow to recover.

A common pattern is to combine windows, for example warning on a long window and critical on a
short one (or vice versa), so that the check both catches sustained problems and ignores brief
spikes. The default check (`5m`, `1m`, `5s`) is an example of this approach.

Because the values are averages, they will not match the instantaneous CPU load shown by tools such
as `top` at the moment the check is executed, and very short spikes that fall between collection
ticks may be missed entirely.

**Interaction with the `disable` setting (Windows only)**

On Windows the collector that feeds this buffer can be turned off with `disable = cpu` in
`[/settings/system/windows]`. In that case `check_cpu` returns UNKNOWN with an explanatory message
rather than reporting values from a buffer that is no longer updated. The entries in `disable` are
matched as whole tokens, so `disable = cpu_frequency` only disables the CPU frequency collector and
leaves `check_cpu` unaffected.

The Linux module has no `disable` setting; its collector is always running.
