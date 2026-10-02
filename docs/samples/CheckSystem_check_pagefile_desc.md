#### About `check_pagefile`

`check_pagefile` reports the size and usage of the system's paging space. It
returns one record per paging file (Windows) or swap device (Linux), plus a
synthetic **`total`** record aggregating all of them — which is what you almost
always want to threshold on, since a host with several pagefiles will otherwise
alert per file.

The defaults are `used > 60%` for warning and `used > 80%` for critical.
Thresholds accept both absolute sizes and percentages, so
`crit=used > 8G` and `crit=used > 80%` are both valid; `free_pct` / `used_pct`
are available when you want the percentage as a plain number.

##### What it does and does not tell you

This is a **capacity** check: how much paging space is committed, not how hard
the machine is paging. A box can sit with swap 90% full and be perfectly
healthy — pages written out long ago and never needed again — while a box with
5% swap used can be thrashing badly. For the pressure signal, use
[`check_swap_io`](#check_swap_io), which reports the paging *rate*, and read the
two together.

##### Windows

`peak_used` reports the high-water mark of commit charge for each pagefile since
boot. That is often the more useful alert than instantaneous usage: it catches
the nightly job that briefly exhausted the pagefile hours before the check ran.

```
check_pagefile "crit=peak_used > 90%"
```

##### Linux

Each swap device (or swap file) is one record, and `name` is its path. A host
with swap disabled entirely reports only the `total` record with a size of zero;
guard against that with `filter=size > 0` if a zero-sized total would otherwise
read as 100% used in your dashboards.

##### macOS

Swap comes from `vm.swapusage`, reported as the single `total` record. macOS
creates its swap files on demand, so a Mac that has not needed swap reports a
size of zero, and the same `filter=size > 0` guard applies.

#### Right after the agent starts (Linux and macOS)

On Linux and macOS the figures come from the 1 Hz background collector rather
than from a direct read, so for the first second after the agent (or the module)
starts there is nothing to report yet:

```
check_pagefile
UNKNOWN: No pagefile/swap data available yet (collector still initializing)
```

`warmup-state` picks the status reported during that window (`ok`, `warning`,
`critical` or `unknown`, the default); the message stays the same. A collector
that has tried to sample and failed - an unreadable `/proc` inside a locked-down
container, for instance - is not warming up: the check then answers UNKNOWN
*"No pagefile/swap data available: the collector failed to sample it: ..."* whatever
`warmup-state` says. On Windows `check_pagefile` reads the system directly and has no
warm-up, so the option is not accepted there. See
[Collector-backed checks and warm-up](../../concepts/checks.md#8-collector-backed-checks-and-warm-up).
