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

### Infrastructure / System Health

| Scenario | Description |
|---|---|
| [Windows Server Health](windows-server-health.md) | Monitor CPU, memory, disk, and uptime together as a baseline health check |
| [Disk Space Alerting](disk-space.md) | Alert when drives are running low on free space |
| [Service & Process Monitoring](service-monitoring.md) | Ensure critical Windows services and processes are running |
| [Event Log Monitoring](event-log.md) | Alert on errors and warnings in the Windows Event Log |

### Network

| Scenario | Description |
|---|---|
| [Network Checks](network-checks.md) | Check host reachability (ping), TCP port availability, and HTTP endpoint health |

### Extensibility

| Scenario | Description |
|---|---|
| [External Scripts](external-scripts.md) | Run your own scripts and batch files as monitoring checks |
| [Passive Monitoring (NSCA/NRDP)](passive-monitoring.md) | Have NSClient++ push results to your monitoring server on a schedule |

---

## Not sure where to start?

If this is your first time, read the [Quick Start](../quick-start.md) guide first, then come back here and pick the scenario that matches what you need to monitor.

To understand the filter and threshold engine that all checks share, see [Checks In Depth](../checks-in-depth/index.md).
