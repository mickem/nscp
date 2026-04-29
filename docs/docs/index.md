
# NSClient++

**NSClient++** is a simple, powerful, and secure monitoring agent for Windows (and Linux). It integrates with any monitoring solution that speaks NRPE, NSCA, REST, or check_mk — including Nagios, Icinga, Op5, and many others.

---

## Where do you want to go?

### 🚀 I'm new — get me started fast
Follow the [Quick Start guide](quick-start.md) to install NSClient++, run your first check, and understand the output in about 10 minutes.

### 📋 I want to monitor something specific
Browse the [Monitoring Scenarios](scenarios/index.md) for step-by-step guides covering the most common real-world monitoring tasks:

- [Windows Server Health](scenarios/windows-server-health.md) — CPU, memory, disk, uptime
- [Disk Space Alerting](scenarios/disk-space.md)
- [Service & Process Monitoring](scenarios/service-monitoring.md)
- [Event Log Monitoring](scenarios/event-log.md)
- [Network Checks](scenarios/network-checks.md) — ping, TCP, HTTP
- [External Scripts](scenarios/external-scripts.md)
- [Passive Monitoring (NSCA/NRDP)](scenarios/passive-monitoring.md)

### 🔍 I need reference material
The [Reference section](reference/index.md) has complete documentation for every module, command, and configuration option.

### 🎓 I want to understand how it all works
Read [Concepts: How NSClient++ Works](concepts/index.md) to understand modules, commands, protocols, and the filter/threshold engine that all checks share.

---

## Quick Reference

The most common check commands with their default thresholds:

| Command | What it checks | Default warn | Default crit |
|---|---|---|---|
| `check_cpu` | CPU load | >80% | >90% |
| `check_memory` | Memory usage | >79% | >89% |
| `check_drivesize` | Disk space | >79% used | >89% used |
| `check_service` | Windows services | any auto-start stopped | — |
| `check_process` | Process running | process not found | — |
| `check_eventlog` | Windows event log | warning level entries | error/critical entries |
| `check_uptime` | Time since reboot | — | — |
| `check_ping` | Ping response | >60ms or >5% loss | >100ms or >10% loss |

---

## Supported Platforms

- **Windows**: Windows 10 / Server 2016 and later (Win32 and x64)
- **Linux**: Debian, Ubuntu, CentOS/RHEL (limited module support)

For older OS support, see the [FAQ](faq.md).
