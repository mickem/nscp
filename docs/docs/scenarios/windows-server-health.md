# Windows Server Health Monitoring

**Goal:** Set up baseline health monitoring for a Windows server — CPU load, memory usage, disk space, and uptime — in a single, consistent configuration.

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
nscp settings --activate-module CheckSystem
nscp settings --activate-module CheckDisk
```

---

## CPU Load

### Command

```
check_cpu
```

### Expected output (healthy)

```
OK: CPU load is ok.
'total 5m'=2%;80;90 'total 1m'=5%;80;90 'total 5s'=11%;80;90
```

### Expected output (alert)

```
WARNING: WARNING: 5m: 85%, 1m: 88%, 5s: 91%
'total 5m'=85%;80;90 'total 1m'=88%;80;90 'total 5s'=91%;80;90
```

### How it works

By default `check_cpu` reports CPU load over three time windows (5 seconds, 1 minute, 5 minutes). The default warning threshold is 80% and critical is 90%, applied to the total CPU load.

### Customisation

**Change warning and critical thresholds:**

```
check_cpu "warn=load > 70" "crit=load > 85"
```

**Check only the 5-minute average:**

```
check_cpu time=5m "warn=load > 80" "crit=load > 90"
```

**Include per-core data (remove the default `total`-only filter):**

```
check_cpu filter=none "warn=load > 80" "crit=load > 90"
```

**Include kernel time in the alert condition:**

```
check_cpu filter=none "warn=kernel > 10 or load > 80" "crit=load > 90"
```

**Via NRPE from your monitoring server:**

```
check_nrpe -H <agent-ip> -c check_cpu
```

---

## Memory Usage

### Command

```
check_memory
```

### Expected output (healthy)

```
OK memory within bounds.
'page used'=8G;19;21 'page used %'=33%;79;89 'physical used'=7G;9;10 'physical used %'=65%;79;89
```

### How it works

`check_memory` checks both physical RAM and the page file (virtual memory). The defaults warn at 79% used and go critical at 89% used.

### Customisation

**Warn on free memory rather than used:**

```
check_memory "warn=free < 20%" "crit=free < 10%"
```

**Show detail in the message:**

```
check_memory "top-syntax=${list}" "detail-syntax=${type} free: ${free} used: ${used} size: ${size}"
```

Example output:
```
page free: 16G used: 7.98G size: 24G, physical free: 4.18G used: 7.8G size: 12G
```

**Lock performance data units to gigabytes** (prevents auto-scaling between checks):

```
check_memory "perf-config=*(unit:G)"
```

**Via NRPE:**

```
check_nrpe -H <agent-ip> -c check_memory
```

---

## Disk Space

### Command

```
check_drivesize
```

### Expected output (healthy)

```
OK: All drives ok
'C:\ used'=45GB;178;200;0;223 'C:\ used %'=20%;79;89;0;100
```

### Expected output (alert)

```
CRITICAL: CRITICAL: C:\: 205GB/223GB used
'C:\ used'=205GB;178;200;0;223 'C:\ used %'=91%;79;89;0;100
```

### How it works

`check_drivesize` checks all local fixed drives by default and warns at 79% used, critical at 89% used.

### Customisation

**Check only drive C: with custom thresholds (free space):**

```
check_drivesize drive=C: "warn=free < 20%" "crit=free < 10%"
```

**Check all drives:**

```
check_drivesize drive=* "warn=free < 10%" "crit=free < 5%"
```

**Check only fixed and network drives, exclude C: and D::**

```
check_drivesize drive=* "filter=type in ('fixed', 'remote')" exclude=C:\ exclude=D:\
```

**Force performance data in gigabytes:**

```
check_drivesize "perf-config=*(unit:G)"
```

**Via NRPE:**

```
check_nrpe -H <agent-ip> -c check_drivesize
```

See also the full [Disk Space scenario](disk-space.md) for more examples.

---

## Network Interfaces

### Command

```
check_network
```

### Expected output (healthy)

```
OK: Network interfaces seem ok.
'Ethernet 1'=1024B;10000;100000 'Ethernet 2'=512B;10000;100000
```

### Expected output (alert)

```
CRITICAL: Ethernet 1 >50000 <200000 bps
```

### How it works

`check_network` reports per-NIC throughput in bytes/sec by polling Windows
performance counters in the background and computing a rate from successive
samples. The default thresholds are `total > 10000` (warn) and
`total > 100000` (critical) on total bytes/sec.

By default the check reads `Win32_PerfRawData_Tcpip_NetworkInterface`, which
exposes one row per *physical* adapter. If you use **NIC teaming** (LBFO,
Switch-Embedded Teaming, or any other Windows team configuration) and want to
monitor the aggregated team interface rather than the individual members, use
`mode=adapter` to read from `Win32_PerfRawData_Tcpip_NetworkAdapter` instead.
That class also reports the team adapter (e.g. a virtual interface named after
the team itself).

### Customisation

**Monitor the team aggregate on a server with NIC teaming:**

```
check_network mode=adapter "warn=total > 100M" "crit=total > 500M"
```

**Alert only on the team adapter (ignore individual physical members):**

The team aggregate is the only row with no MAC address (no matching
`Win32_NetworkAdapter` entry exists), so filter on that:

```
check_network mode=adapter "filter=MAC = ''"
```

**Report from both sources at once** (the `source` keyword distinguishes them):

```
check_network mode=both "filter=source = 'adapter' or status = '2'"
```

**Higher thresholds for a busy 10G uplink:**

```
check_network "filter=name = 'Ethernet 1'" "warn=total > 800M" "crit=total > 1G"
```

**Custom output showing send and receive separately:**

```
check_network "detail-syntax=${name}: tx=${sent}/s rx=${received}/s"
```

### Choosing a mode

| Mode        | Source                                          | Includes team aggregate? | When to use                                                                       |
|-------------|-------------------------------------------------|--------------------------|-----------------------------------------------------------------------------------|
| `interface` | `Win32_PerfRawData_Tcpip_NetworkInterface`      | No                       | Default. Standalone servers, or when you only care about the physical NICs.       |
| `adapter`   | `Win32_PerfRawData_Tcpip_NetworkAdapter`        | Yes                      | NIC-teamed servers where you want the team aggregate, not just the team members.  |
| `both`      | Both classes, every row tagged with `source=`   | Yes                      | When you need a single check to alert on both the team aggregate and its members. |

<!-- @formatter:off -->
!!! note
    Switching from `interface` to `adapter` changes the **names** of the
    reported interfaces (NetworkAdapter uses the friendly Windows name,
    NetworkInterface uses the MIB-style name). Dashboards or thresholds that
    reference a specific adapter by name may need to be updated.
<!-- @formatter:on -->

**Via NRPE:**

```
check_nrpe -H <agent-ip> -c check_network --argument "mode=adapter"
```

---

## System Uptime

### Command

```
check_uptime
```

### Expected output

```
uptime: 9:02, boot: 2024-03-15 08:29:13
'uptime'=32531s
```

### How it works

`check_uptime` reports how long since the last reboot. By default it returns `OK` with no threshold. Use it to detect unexpectedly short uptime (i.e., a machine that rebooted when it shouldn't have).

### Customisation

**Alert if the server rebooted in the last 24 hours:**

```
check_uptime "warn=uptime < 1d" "crit=uptime < 1h"
```

**Via NRPE:**

```
check_nrpe -H <agent-ip> -c check_uptime
```

---

## Putting It All Together

Here is a minimal `nsclient.ini` that enables all four checks and exposes them via NRPE:

```ini
[/modules]
CheckSystem = enabled
CheckDisk   = enabled
NRPEServer  = enabled

[/settings/NRPE/server]
allowed hosts = 10.0.0.1    ; IP of your monitoring server
port          = 5666
```

On your monitoring server (Nagios/Icinga/Op5), define service checks:

```
check_nrpe -H <agent-ip> -c check_cpu
check_nrpe -H <agent-ip> -c check_memory
check_nrpe -H <agent-ip> -c check_drivesize
check_nrpe -H <agent-ip> -c check_uptime
```

<!-- @formatter:off -->
!!! tip
    To run these checks on a schedule and push the results passively (without polling), see the [Passive Monitoring scenario](passive-monitoring-nsca.md).
<!-- @formatter:on -->

---

## Next Steps

- [Disk Space Alerting](disk-space.md) — more disk check options
- [Service & Process Monitoring](service-monitoring.md) — check that critical services are running
- [Event Log Monitoring](event-log.md) — catch errors before they become incidents
- [Checks In Depth: Filters](../concepts/checks.md#3-filters-choosing-what-to-check) — master the filter and threshold syntax
