# Linux Server Health Monitoring

**Goal:** Set up baseline health monitoring for a Linux server — load, CPU
utilization, memory, swap activity, kernel activity, disk space and services —
using the Linux-native checks in `CheckSystem` and `CheckDisk`.

---

## Prerequisites

Enable the following modules in `nsclient.ini`:

```ini
[/modules]
CheckSystem = enabled
CheckDisk   = enabled
NRPEServer  = enabled   ; if using NRPE (active monitoring)
```

Or activate them from the command line:

```
nscp settings --activate-module CheckSystem CheckDisk
```

---

## Load Average

`check_load` reports the 1/5/15-minute run-queue averages from `/proc/loadavg`.

```
check_load "warn=load5 > 4" "crit=load5 > 8"
OK: total load average: 0.96, 0.79, 0.75
```

`load` is a convenience keyword equal to the highest of the three windows. On
hosts of different sizes, use `percpu=true` to normalise by core count (the row
then reports as `scaled`), so a single threshold ports everywhere:

```
check_load percpu=true "warn=load > 1" "crit=load > 2"
```

---

## CPU Utilization

`check_cpu_utilization` takes a fresh ~1-second sample of `/proc/stat` and breaks
the CPU's time down by mode — useful to distinguish user load from I/O wait or
hypervisor steal.

```
check_cpu_utilization
OK: user: 1.39% system: 1.54% iowait: 0% steal: 0% idle: 95.73%
```

Default thresholds are `total > 90` (warning) / `total > 95` (critical). Target a
specific pressure source directly:

```
check_cpu_utilization "warn=iowait > 20" "crit=iowait > 50"   ; storage saturation
check_cpu_utilization "warn=steal > 5"  "crit=steal > 15"     ; noisy-neighbour VM
```

For rolling averaged CPU load over 1m/5m/15m windows use `check_cpu` instead;
`check_cpu_utilization` is the instantaneous per-mode breakdown.

---

## Memory and Swap

`check_memory` covers RAM/commit; on Linux, pair it with `check_swap_io` to catch
*active* swapping, which is often a better pressure signal than swap usage:

```
check_memory "warn=used > 80%" "crit=used > 90%"
check_swap_io "warn=swap_out > 100" "crit=swap_out > 1000"
OK: 1 swap device(s) in 0 pages/s, out 0 pages/s
```

Sustained non-zero `swap_out` (pages/second) means the box is under real memory
pressure. `swap_in_bytes`/`swap_out_bytes` give the same figures in bytes/second.

---

## Kernel Activity

`check_kernel_stats` reports the context-switch rate, fork rate and live thread
count — good for spotting runaway thread creation or a switch/fork storm.

```
check_kernel_stats
OK - Context Switches 57111.0/s, Process Creations 317.0/s, Threads 363
```

The default alerts on the thread count (`current > 8000` / `10000`). To watch a
rate instead:

```
check_kernel_stats "warn=name = 'ctxt' and rate > 100000" "crit=name = 'ctxt' and rate > 500000"
```

---

## Disk Space and Mounts

```
check_drivesize drive=/ "warn=free < 20%" "crit=free < 10%"
check_drivesize drive=/ "warn=inodes_used_pct > 85" "crit=inodes_used_pct > 95"
check_mount mount=/data fstype=ext4
```

See [Disk Space Alerting](disk-space.md) for the full walkthrough, including
inode exhaustion and mount verification.

---

## Services

On Linux `check_service` inspects **systemd** units. By default it flags any
enabled unit that has failed or stopped, while ignoring units that are
deliberately disabled:

```
check_service
OK: All 42 service(s) are ok.
```

Check a specific unit and show its state, raw systemd state and vendor preset:

```
check_service service=cron "top-syntax=${list}" "detail-syntax=${name}=${state} active=${active} preset=${preset}"
cron=running active=active preset=enabled
```

Process metrics are available too (`rss`, `vms`, `cpu`, `tasks`, `age`), e.g.
`check_service service=mysql "crit=rss > 2G"`. See
[Service & Process Monitoring](service-monitoring.md).

---

## Identifying the Host

`check_os_version` reports the distribution and kernel — handy for inventory and
for confirming the agent is on the box you think it is:

```
check_os_version
OK: Ubuntu 24.04.1 LTS (kernel 6.6.87.2-microsoft-standard-WSL2)
```

---

## Putting It All Together

A minimal `nsclient.ini` for a monitored Linux host:

```ini
[/modules]
CheckSystem = enabled
CheckDisk   = enabled
NRPEServer  = enabled

[/settings/NRPE/server]
allowed hosts = 10.0.0.1        ; your monitoring server
allow arguments = false         ; define checks below instead of passing args

[/settings/external scripts/alias]
alias_load        = check_load "warn=load5 > 4" "crit=load5 > 8"
alias_cpu         = check_cpu_utilization "warn=total > 90" "crit=total > 95"
alias_mem         = check_memory "warn=used > 80%" "crit=used > 90%"
alias_swap        = check_swap_io "warn=swap_out > 100" "crit=swap_out > 1000"
alias_disk        = check_drivesize drive=/ "warn=free < 20%" "crit=free < 10%"
alias_services    = check_service
```

Each alias is then callable by name from the monitoring server over NRPE
(`check_nrpe -H <agent> -c alias_load`).

---

## Next Steps

- [Service & Process Monitoring](service-monitoring.md)
- [Disk Space Alerting](disk-space.md)
- [Network Checks](network-checks.md)
- [Active Monitoring with NRPE](nrpe.md)
