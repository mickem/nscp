# Quick Start

This guide takes you from zero to your first successful check in about 10 minutes.

!!! note
    This guide assumes Windows. NSClient++ also runs on Linux, but some modules are Windows-only.

---

## Step 1 — Download and Install

Download the latest MSI installer from the [releases page](https://github.com/mickem/nscp/releases).
Look for a file named `NSCP-<version>-x64.msi` and launch it.

The installer will walk you through a few screens:

1. **Select monitoring tool** — choose `Generic` if you are not using Op5 Monitor.
2. **Select configuration** — the default (ini file) is the right choice for getting started.
3. **Basic settings** — enter the IP address of your monitoring server in the **Allowed hosts** field and note the generated password; you will need it to access the web interface.
4. **Enable common checks** — leave this checked so that CPU, memory, and disk checks are available immediately.
5. **Enable NRPE server** — enable this if you want your monitoring server to poll NSClient++ using `check_nrpe`.
6. Click **Install**.

After the installer finishes, the `nscp` Windows service starts automatically.

---

## Step 2 — Open the Test Shell

The test shell lets you run checks interactively and see debug output in real time. It is the best tool for learning and troubleshooting.

!!! warning
    The test shell runs NSClient++ in the foreground. Stop the service first so they do not conflict.

Open an **Administrator command prompt** and run:

```
cd "C:\Program Files\NSClient++"
net stop nscp
nscp test
```

You will see startup messages followed by:

```
L     client Enter command to inject or exit to terminate...
```

You are now in the test shell. Type `exit` at any time to quit, then restart the service with `net start nscp`.

---

## Step 3 — Load a Module and Run Your First Check

Modules provide the actual check commands. If you enabled "common checks" during installation, the key modules are already active. Let's verify by running a CPU check:

```
check_cpu
```

Expected output:

```
OK: CPU load is ok.
'total 5m'=2%;80;90 'total 1m'=5%;80;90 'total 5s'=11%;80;90
```

If you instead see `Unknown command(s): check_cpu`, load the module first:

```
load CheckSystem
check_cpu
```

**Understanding the output:**

```
OK: CPU load is ok.
'total 5m'=2%;80;90   ← 5-minute average: 2% used, warn at 80%, crit at 90%
'total 1m'=5%;80;90   ← 1-minute average
'total 5s'=11%;80;90  ← 5-second average
```

- The status (`OK`, `WARNING`, `CRITICAL`, `UNKNOWN`) is what your monitoring server receives.
- The `'key'=value;warn;crit` pairs are **performance data** — metrics your monitoring server can graph over time.

---

## Step 4 — Try a Few More Checks

```
check_memory
check_drivesize
check_service
check_uptime
```

Each command works with sensible defaults. See what comes back and experiment with arguments:

```
# Custom thresholds — warn if CPU > 50%, critical if > 70%
check_cpu "warn=load > 50" "crit=load > 70"

# Check only drive C:
check_drivesize drive=C: "warn=free < 20%" "crit=free < 10%"

# Show memory detail
check_memory "top-syntax=${list}" "detail-syntax=${type} free: ${free} used: ${used} size: ${size}"
```

---

## Step 5 — What Next?

Now that NSClient++ is running and you have run your first checks, choose a path:

| I want to… | Go to… |
|---|---|
| Monitor a real server end-to-end | [Windows Server Health scenario](scenarios/windows-server-health.md) |
| Set up disk space alerting | [Disk Space scenario](scenarios/disk-space.md) |
| Monitor a Windows service | [Service Monitoring scenario](scenarios/service-monitoring.md) |
| Monitor the Windows event log | [Event Log scenario](scenarios/event-log.md) |
| Connect to my monitoring server via NRPE | [Getting Started: NRPE](getting-started/nrpe.md) |
| Connect via passive monitoring (NSCA) | [Getting Started: NSCA](getting-started/nsca.md) |
| Understand filters and thresholds | [Checks In Depth](checks-in-depth/index.md) |
| Understand how modules work | [Concepts: How NSClient++ Works](concepts/index.md) |
