# How NSClient++ Works

Understanding the core concepts of NSClient++ will help you configure it correctly and troubleshoot problems quickly.

---

## The Big Picture

NSClient++ is a **monitoring agent** — a small service that runs on the machine you want to monitor. It does two things:

1. **Responds to requests** from a central monitoring server (active/polling mode).
2. **Pushes results** to a central monitoring server on a schedule (passive mode).

```
  Monitoring Server                  Monitored Machine
  (Nagios, Icinga, Op5…)             (Windows / Linux)

  check_nrpe ──────────────────────► NSClient++
             ◄──────── check result ─┘  (active/NRPE)

  NSCAClient ◄──────── check result ─── NSClient++ + Scheduler
             (passive/NSCA, NRDP)
```

---

## Modules

Everything in NSClient++ is provided by **modules**. A module is a plugin that you load to enable specific capabilities.

**Modules must be explicitly enabled** — nothing runs unless it is loaded. This keeps the agent lightweight and its attack surface small.

### How to enable a module

In your `nsclient.ini` configuration file, add the module to the `[/modules]` section:

```ini
[/modules]
CheckSystem = enabled
CheckDisk   = enabled
NRPEServer  = enabled
```

You can also enable a module from the command line without editing the file:

```
nscp settings --activate-module CheckSystem
```

Or load it temporarily in the test shell without touching the configuration:

```
nscp test
load CheckSystem
```

### What happens if you forget to load a module?

You will see an error like this:

```
UNKNOWN: Unknown command(s): check_cpu available commands: ...
```

The fix is always the same: load the module that provides the command you need.

---

## Commands (Checks)

Each module provides one or more **commands** (also called queries or checks). A command is what your monitoring server calls, and what produces a status and performance data.

| Module                 | Commands provided                                                                             |
|------------------------|-----------------------------------------------------------------------------------------------|
| `CheckSystem`          | `check_cpu`, `check_memory`, `check_service`, `check_process`, `check_uptime`, `check_pdh`, … |
| `CheckDisk`            | `check_drivesize`, `check_files`                                                              |
| `CheckEventLog`        | `check_eventlog`                                                                              |
| `CheckNet`             | `check_ping`, `check_tcp`, `check_http`, `check_dns`                                          |
| `CheckExternalScripts` | Any script you add to the config                                                              |
| `CheckWMI`             | `check_wmi`                                                                                   |
| `CheckTaskSched`       | `check_tasksched`                                                                             |
| `NRPEServer`           | Accepts incoming connections from `check_nrpe`                                                |
| `NSCAClient`           | Pushes passive results to an NSCA server                                                      |
| `Scheduler`            | Runs commands on a timer for passive monitoring                                               |

For a complete reference see the [Reference section](../reference/index.md).

---

## Check Results

Every check returns three things:

1. **Status** — `OK`, `WARNING`, `CRITICAL`, or `UNKNOWN`
2. **Message** — human-readable text explaining the status
3. **Performance data** — machine-readable key=value metrics for graphing

Example:

```
OK: CPU load is ok.
'total 5m'=2%;80;90 'total 1m'=5%;80;90 'total 5s'=11%;80;90
```

- `OK` is the status
- `CPU load is ok.` is the message
- `'total 5m'=2%;80;90` means: metric named `total 5m`, value `2%`, warn threshold `80%`, crit threshold `90%`

---

## Filters, Thresholds, and Syntax

All NSClient++ checks share the same engine for filtering, thresholds, and output formatting. Understanding this once unlocks all checks.

- **`filter`** — SQL-like expression that selects which items to include (e.g., `filter=core = 'total'`)
- **`warn`** / **`crit`** — expressions that trigger warning or critical status (e.g., `warn=load > 80`)
- **`top-syntax`** / **`detail-syntax`** — templates that control what the message looks like

See [Checks In Depth](checks.md) for a full guide to these options.

---

## Protocols

NSClient++ speaks many protocols. The most common are:

| Protocol        | Direction              | Use case                                 |
|-----------------|------------------------|------------------------------------------|
| **NRPE**        | Server polls agent     | Active monitoring with Nagios/Icinga/Op5 |
| **NSCA / NRDP** | Agent pushes to server | Passive monitoring                       |
| **REST**        | Both                   | NSClient++ native protocol, web UI       |
| **Graphite**    | Agent pushes metrics   | Real-time graphing                       |
| **check_mk**    | Server polls agent     | Check_MK monitoring                      |

See [Getting Started](../getting-started/index.md) for step-by-step protocol setup.

---

## Configuration

NSClient++ is configured via an INI-format file (`nsclient.ini`) located in the installation directory.

The configuration has a hierarchical structure:

```ini
[/modules]
CheckSystem = enabled     ; load the CheckSystem module

[/settings/NRPE/server]
allowed hosts = 10.0.0.1  ; which hosts can connect
port = 5666               ; which port to listen on

[/settings/default]
password = secret         ; web UI / check_nt password
```

To see all available settings for your current configuration:

```
nscp settings --generate --add-defaults
```

To remove all default values (shorter file):

```
nscp settings --generate --remove-defaults
```

To re-write the configuration file with sections (and keys within each
section) sorted alphabetically:

```
nscp settings --sort
```

`[/modules]` is kept as the first section. Sorting is opt-in — the
regular `--generate`/`--update` commands preserve the existing order.

!!! tip
    Use the [Web UI](../web.md) to explore and change settings interactively — it shows descriptions for every option.

---

## Summary

| Concept   | What it means                                            |
|-----------|----------------------------------------------------------|
| Module    | A plugin that must be loaded to enable its commands      |
| Command   | A check that returns status + message + performance data |
| Filter    | An expression that selects which items to check          |
| Threshold | A `warn=` or `crit=` expression that triggers an alert   |
| Protocol  | How NSClient++ talks to your monitoring server           |

**Next steps:**

- [Quick Start](../quick-start.md) — Install and run your first check
- [Monitoring Scenarios](../scenarios/index.md) — Real-world examples
- [Checks In Depth](checks.md) — Master filters and thresholds
