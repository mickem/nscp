# Event Log Monitoring

**Goal:** Alert on errors, warnings, and specific events in the Windows Event Log — both by polling and in real time.

---

## Prerequisites

Enable the `CheckEventLog` module in `nsclient.ini`:

```ini
[/modules]
CheckEventLog = enabled
NRPEServer    = enabled   ; if using NRPE
```

---

## Basic Event Log Check

### Command

```
check_eventlog
```

### Expected output (healthy)

```
OK: No problems found.
'problem_count'=0;0;0
```

### Expected output (alert)

```
CRITICAL: CRITICAL: 5 message(s) Application Bonjour Service (Task Scheduling Error: ...)
'problem_count'=5;0;0
```

### How it works

By default, `check_eventlog` scans the `Application`, `System`, and `Security` logs for events in the **past 24 hours** with a level of `warning`, `error`, or `critical`. Warnings trigger a WARNING status; errors and critical events trigger CRITICAL.

Default filter values:

| Parameter | Default value |
|---|---|
| `filter` | `level in ('warning', 'error', 'critical')` |
| `warn` | `level = 'warning' or problem_count > 0` |
| `crit` | `level in ('error', 'critical')` |
| `scan-range` | `-24h` (last 24 hours) |

---

## Common Scenarios

### Change the time window

Check events from the last 6 hours:

```
check_eventlog scan-range=-6h
```

Check events from the last week:

```
check_eventlog scan-range=-1w
```

Warn on events from the past week, but critical only for events in the past 24 hours:

```
check_eventlog scan-range=-1w "warn=count gt 0" "crit=written > -24h"
```

### Check only specific event log levels

Alert only on errors (not warnings):

```
check_eventlog "filter=level = 'error'"
```

**Event level reference:**

| Level | NSClient++ keyword |
|---|---|
| 1 | `critical` |
| 2 | `error` |
| 3 | `warning`, `warn` |
| 4 | `informational`, `info`, `success`, `auditSuccess` |
| 5 | `debug`, `verbose` |
| Any number | Use the number directly: `level = 42` |

### Find a specific event by Provider and Event ID

This is the recommended approach — it is fast and unambiguous:

```
check_eventlog "filter=provider = 'Microsoft-Windows-Security-SPP' and id = 903"
```

To also match on message content:

```
check_eventlog "filter=provider = 'Microsoft-Windows-Security-SPP' and id = 903 and message like 'expired'"
```

!!! tip
    To find the provider name and event ID, open **Event Viewer**, click on the event, and look in the **Details** tab or the **General** description header.

### Check a specific event log channel

Since NSClient++ 0.4.2, you can read any channel on modern Windows:

```
check_eventlog "file=Microsoft-Windows-AAD/Operational" scan-range=-100w show-all filter=none
```

!!! note
    Channel names use `-` as a folder separator rather than `\` or `/`. Find the full channel name by right-clicking the channel in Event Viewer and selecting **Properties**.

### Find non-error messages

To find informational messages (which the default filter excludes), disable the filter:

```
check_eventlog filter=none "filter=level = 'informational'"
```

---

## Real-Time Monitoring

Polling is simple but can miss brief events between checks. Real-time monitoring catches events the moment they are written to the log and immediately pushes them to the monitoring server (via NSCA/NRDP).

### Enable real-time filtering

```ini
[/modules]
CheckEventLog = enabled

[/settings/eventlog/real-time]
enabled = true
```

### Add a filter

This example catches all `error` level events in the Application log and sends them to the `NSCA` channel:

```ini
[/settings/eventlog/real-time/filters/app_errors]
log         = application
destination = NSCA
filter      = level = 'error'
maximum age = 30s
```

Configuration options:

| Key | Description |
|---|---|
| `log` | The log to monitor (`application`, `system`, `security`, or a channel path) |
| `destination` | Where to send alerts (`NSCA`, `NRDP`, `log`) |
| `filter` | Expression to match against log entries |
| `maximum age` | If no alert has been sent, send an `OK` heartbeat every N seconds |

### Test by injecting a log entry

```
eventcreate /ID 1 /L application /T ERROR /SO MYEVENTSOURCE /D "Test error message"
```

---

## Configuration Example

Full `nsclient.ini` for NRPE-based event log monitoring:

```ini
[/modules]
CheckEventLog = enabled
NRPEServer    = enabled

[/settings/NRPE/server]
allowed hosts = 10.0.0.1
port          = 5666
```

On the monitoring server:

```
check_nrpe -H <agent-ip> -c check_eventlog
```

---

## Next Steps

- [Passive Monitoring](passive-monitoring.md) — use NSCA/NRDP to push real-time event log alerts
- [Service & Process Monitoring](service-monitoring.md) — correlate service failures with event log errors
- [Checks In Depth: Filters](../checks-in-depth/index.md#4-filters-choosing-what-to-check) — master filter expressions
- [Reference: CheckEventLog](../reference/windows/CheckEventLog.md) — full command reference
