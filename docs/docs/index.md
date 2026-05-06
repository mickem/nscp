
# NSClient++

**NSClient++** is a simple, powerful, and secure monitoring agent for Windows (and Linux). It integrates with any monitoring solution that speaks NRPE, NSCA, REST, or check_mk — including Nagios, Icinga, Op5, and many others.

---

## Where do you want to go?

### 🚀 I'm new — get me started fast
Follow the [Quick Start guide](quick-start.md) to install NSClient++, run your first check, and understand the output in about 10 minutes.

### 📦 I want to install or upgrade
[Installing NSClient++](setup/installing.md) — interactive MSI walkthrough, automated/silent install, MSI options, monitoring-tool-specific configuration, and pulling configuration from a remote HTTP server.

### 🌐 I want to manage the agent from the browser
[Using the Web Interface](setup/web-interface.md) — enable the built-in web server, log in, run queries, and edit settings without the command line.

### 📋 I want to monitor something specific
Browse the [Monitoring Scenarios](scenarios/index.md) for step-by-step guides covering the most common real-world monitoring tasks:

**System health**

- [Windows Server Health](scenarios/windows-server-health.md) — CPU, memory, disk, uptime
- [Disk Space Alerting](scenarios/disk-space.md)
- [Service & Process Monitoring](scenarios/service-monitoring.md)
- [Event Log Monitoring](scenarios/event-log.md)
- [Performance Counter (PDH) Monitoring](scenarios/counters.md) — read Windows performance counters

**Network**

- [Network Checks](scenarios/network-checks.md) — ping, TCP, HTTP, DNS

**Custom logic**

- [External Scripts](scenarios/external-scripts.md) — wrap PowerShell/batch/VBScript checks

### 🔌 I want to connect NSClient++ to my monitoring server
Pick the protocol that matches your setup:

- [Active Monitoring with NRPE](scenarios/nrpe.md) — let the monitoring server poll the agent (Nagios-style)
- [Passive Monitoring (NSCA/NRDP)](scenarios/passive-monitoring-nsca.md) — push results on a schedule (NSCA or HTTP-based NRDP)
- [Passive Monitoring (Icinga 2)](scenarios/passive-monitoring-icinga.md) — submit results to the Icinga 2 REST API
- [Prometheus Scraping](scenarios/prometheus.md) — let Prometheus pull raw metrics from the agent's OpenMetrics endpoint

### 🔒 I want to harden the agent
[Securing NSClient++](setup/securing.md) — TLS configuration, two-way authentication with client certificates, and protocol-specific hardening guidance.

### 🔍 I need reference material
The [Reference section](reference/index.md) has complete documentation for every module, command, and configuration option.

### 🎓 I want to understand how it all works
Read [Concepts: How NSClient++ Works](concepts/index.md) to understand modules, commands, and protocols. The filter/threshold engine that every check shares — plus the test-mode shell, `perf-config` reference, and end-to-end examples — is in [Checks In Depth](concepts/checks.md).

### 🆘 I have a problem or a question
The [FAQ](faq.md) covers the common operational issues — timeouts, allowed-hosts, NRPE insecure mode, broken performance counters, escaping rules, and more.

---

## Quick Reference

The most common check commands with their default thresholds:

| Command           | What it checks    | Default warn           | Default crit           |
|-------------------|-------------------|------------------------|------------------------|
| `check_cpu`       | CPU load          | >80%                   | >90%                   |
| `check_memory`    | Memory usage      | >79%                   | >89%                   |
| `check_drivesize` | Disk space        | >79% used              | >89% used              |
| `check_service`   | Windows services  | any auto-start stopped | —                      |
| `check_process`   | Process running   | process not found      | —                      |
| `check_eventlog`  | Windows event log | warning level entries  | error/critical entries |
| `check_uptime`    | Time since reboot | uptime < 2d            | uptime < 1d            |
| `check_ping`      | Ping response     | >60ms or >5% loss      | >100ms or >10% loss    |

---

## Supported Platforms

- **Windows**: Windows 2008 and later (Win32 and x64).
    - For Windows XP / Server 2003, use the legacy 0.4.x branch.
- **Linux**: Debian, Ubuntu, CentOS/RHEL (limited module support).

## Supported Protocols

- **[NRPE](scenarios/nrpe.md)**: Nagios Remote Plugin Executor — the most widely supported agent protocol.
- **[NSCA](scenarios/passive-monitoring-nsca.md)**: Nagios Service Check Acceptor — for passive monitoring (push).
- **[NRDP](scenarios/passive-monitoring-nsca.md#using-nrdp-instead-of-nsca)**: Nagios Remote Data Processor — modern HTTP-based alternative to NSCA.
- **[Icinga 2](scenarios/passive-monitoring-icinga.md)**: Submit scheduled check results to the Icinga 2 REST API.
- **[REST API](api/rest/index.md)**: For custom integrations and scripts.
- **check_mk**: For check_mk users, NSClient++ can be configured to work with the check_mk agent protocol — see [CheckMKClient](reference/check/CheckMKClient.md) / [CheckMKServer](reference/check/CheckMKServer.md) reference.
- **Graphite**: For sending performance data to Graphite/Carbon — see [GraphiteClient](reference/client/GraphiteClient.md) reference.
- **[Prometheus](scenarios/prometheus.md)**: Exposes metrics in OpenMetrics format on `/api/v2/openmetrics` for Prometheus to scrape.

