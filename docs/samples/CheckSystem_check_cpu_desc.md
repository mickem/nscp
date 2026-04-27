#### How CPU load is measured (historical buffer)

`check_cpu` does not measure the CPU load at the moment the check is executed. Instead, NSClient++
runs a background collector thread that samples the CPU load roughly **once per second** and pushes
each sample into an in-memory ring buffer. Whenever you run `check_cpu` the values reported are
**averages computed from this buffer** for one or more time windows.

The time windows are controlled by the `time=` option. The default is to compute three averages:
`5m`, `1m` and `5s` (which is why the default output contains rows like `total 5m load`,
`total 1m load` and `total 5s load`). You can override this with one or more `time=` arguments,
for example `time=10m` or `time=30s time=2m`.

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
