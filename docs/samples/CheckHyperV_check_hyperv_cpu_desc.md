#### About `check_hyperv_cpu`

On a Hyper-V host the ordinary CPU counters (and Task Manager) only see the
root partition: the time the logical processors spend running guests is
invisible to them, so a host that is saturated by its VMs can look idle.
`check_hyperv_cpu` reads the "Hyper-V Hypervisor Logical Processor" counters
instead, which account for every logical processor regardless of which
partition used it.

Each logical processor is one record (`Hv LP 0`, `Hv LP 1`, ...) and a
synthetic `total` record carries the average over all of them (the sum for
`context_switches`). The default filter keeps only `total`; use
`filter=processor != 'total'` to alert on individual logical processors, for
example to catch one VM pinning a single core.

The run-time counters are rates, so the check samples them twice, one second
apart (`averages`, on by default). `averages=false` skips the wait but then
every rate reads 0 — only useful to prove the counters exist.

The percentages are rounded to one decimal, in the detail line and in the
perfdata alike; the perfdata labels are the processor joined to the keyword
(`total_total_run_time`, `Hv LP 3_guest_run_time`).

Reading `total_run_time` against `guest_run_time` and `hypervisor_run_time`
tells the two kinds of load apart: a high hypervisor share with modest guest
time points at scheduling or intercept overhead (many small VMs, nested
virtualisation, storms of timer interrupts) rather than at busy guests.
