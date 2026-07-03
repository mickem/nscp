#### About `check_kernel_stats`

`check_kernel_stats` reports low-level kernel activity as one row per metric:

* **`ctxt`** — context-switch *rate* (switches/second), sampled over ~1s from
  `/proc/stat`.
* **`processes`** — process/fork *rate* (new processes/second), also from
  `/proc/stat`.
* **`threads`** — the current number of live threads, counted from
  `/proc/*/task`. This is an instantaneous count, so its `rate` is `0`.

Use `type=<ctxt|processes|threads>` (repeatable) to restrict which rows are
returned; the default is all three.

Per-row keywords:

| Keyword   | Description                                                   |
|-----------|--------------------------------------------------------------|
| `name`    | `ctxt`, `processes` or `threads`                            |
| `label`   | Human-friendly label (`Context Switches`, `Threads`, …)     |
| `rate`    | Per-second rate (0 for the `threads` row)                   |
| `current` | Current raw value (cumulative counter, or the thread count) |
| `human`   | Human-readable value                                        |

Default thresholds target the thread count: **warning**
`name = 'threads' and current > 8000`, **critical**
`name = 'threads' and current > 10000`. Threshold on `rate` (with a
`name = 'ctxt'` / `'processes'` guard) to alert on switch/fork storms.
