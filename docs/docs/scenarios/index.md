# Monitoring Scenarios

These guides walk you through complete, end-to-end monitoring setups for the most common real-world use cases.

Each scenario follows the same structure:

- **Goal** — what you are trying to achieve
- **Prerequisites** — which modules to enable
- **Configuration** — the minimal `nsclient.ini` changes needed
- **Command** — the exact check command to run
- **Expected output** — what good and alert output look like
- **Customisation** — how to adjust thresholds or extend the check
- **Next steps** — where to go from here

---

## Available Scenarios

<!-- @formatter:off -->
!!! tip "Looking for install / hardening / web UI?"
    Agent setup, TLS hardening, and the web management UI live in the
    [Setup](../setup/installing.md) section.
<!-- @formatter:on -->

### Infrastructure / System Health

| Scenario                                              | Description                                                               |
|-------------------------------------------------------|---------------------------------------------------------------------------|
| [Windows Server Health](windows-server-health.md)     | Monitor CPU, memory, disk, and uptime together as a baseline health check |
| [Linux Server Health](linux-server-health.md)         | Load, CPU, memory/swap, kernel activity, disk and systemd services on Linux |
| [Disk Space Alerting](disk-space.md)                  | Alert when drives are running low on free space (Windows & Linux)         |
| [Service & Process Monitoring](service-monitoring.md) | Ensure critical services (Windows services / Linux systemd units) and processes are running |
| [Real-Time System Monitoring](realtime-monitoring.md) | Push CPU/memory/process alerts the second they happen (Windows & Linux)   |
| [Event Log Monitoring](event-log.md)                  | Alert on errors and warnings in the Windows Event Log                     |
| [Performance Counter (PDH) Monitoring](counters.md)   | Read Windows performance counters, average them over time, and alert      |

### Network

| Scenario                            | Description                                                                     |
|-------------------------------------|---------------------------------------------------------------------------------|
| [Network Checks](network-checks.md) | Ping, TCP/SSH port checks (incl. TLS), HTTP/HTTPS health, DNS, and remote-agent availability |

### Monitoring Server Integration

How NSClient++ exchanges results with your monitoring server — pick the
direction (active vs. passive) and protocol that matches your setup.

| Scenario                                                      | Description                                                                      |
|---------------------------------------------------------------|----------------------------------------------------------------------------------|
| [Active Monitoring with NRPE](nrpe.md)                        | Let the monitoring server poll NSClient++ over NRPE (Nagios-style active checks) |
| [Passive Monitoring (NSCA/NRDP)](passive-monitoring-nsca.md)  | Have NSClient++ push results to your monitoring server on a schedule             |
| [Passive Monitoring (NSCA-NG)](passive-monitoring-nsca-ng.md) | TLS-PSK successor to NSCA — modern crypto, same passive-push pattern             |
| [Passive Monitoring (Icinga 2)](passive-monitoring-icinga.md) | Submit scheduled check results to the Icinga 2 REST API                          |
| [Passive Monitoring (Graphite)](passive-monitoring-graphite.md) | Push perfdata and system metrics to a Graphite/carbon backend for graphing     |
| [Checkmk Agent Integration](check-mk.md)                      | Serve a Checkmk-compatible agent dump from NSClient++ on TCP/6556                |
| [Prometheus Scraping](prometheus.md)                          | Expose metrics on `/api/v2/openmetrics` for Prometheus to scrape                 |

### Extensibility

| Scenario                                | Description                                               |
|-----------------------------------------|-----------------------------------------------------------|
| [External Scripts](external-scripts.md) | Run your own scripts and batch files as monitoring checks |

---

## Not sure where to start?

If this is your first time, read the [Quick Start](../quick-start.md) guide first, then come back here and pick the scenario that matches what you need to monitor.

To understand the filter and threshold engine that all checks share, see [Checks In Depth](../concepts/checks.md).
