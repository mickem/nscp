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
