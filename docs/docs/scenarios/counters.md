# Performance Counter (PDH) Monitoring

**Goal:** Read a Windows Performance Counter (PDH) value, alert on it, and (optionally) check it
against a moving average instead of a single instantaneous sample.

PDH (*Performance Data Helper*) is the legacy Windows performance API. It is older than WMI and has
real warts — counters are localised, sometimes vanish from the registry, and rate counters need two
samples to mean anything — but it remains the only way to read many useful metrics, and once you
know the gotchas it's straightforward.

---

## Prerequisites

Enable the `CheckSystem` module in `nsclient.ini`:

```ini
[/modules]
CheckSystem = enabled
NRPEServer  = enabled   ; if using NRPE
```

---

## Basic Counter Check

### Command

```
check_pdh "counter=\\Processor(_Total)\\% Processor Time"
```

### Expected output (healthy)

```
OK: \\Processor(_Total)\\% Processor Time = 4
'\\Processor(_Total)\\% Processor Time'=4;0;0
```

The counter path uses backslashes; on Unix shells you'll need to double-escape them.

---

## Common Scenarios

### Find a counter (when you don't know the exact name)

NSClient++ ships a built-in PDH browser. Run it from a command prompt on the Windows host:

```
nscp sys -- --list --all
```

Filter for keywords:

```
nscp sys -- --list Disk --all
```

List instances of a counter that has them (e.g. per-disk, per-network-interface):

```
nscp sys -- --expand-path "\\PhysicalDisk(*)\\Avg. Disk Queue Length"
```

### Validate a counter actually returns data

Before wiring a counter into a Nagios check, verify it on the host:

```
nscp sys -- --validate Disk --all
\\PhysicalDisk(_Total)\\Avg. Disk Bytes/Read: ok-rate(0)
\\PhysicalDisk(_Total)\\% Idle Time: ok-rate(0)
```

`ok-rate(N)` means the counter responded; `N` is the sample value. If a counter is broken, the
output flags it here rather than at check time.

### Look up an index ⇄ name (helpful for localisation troubleshooting)

```
nscp sys -- --lookup-index 238       ; index → English name
nscp sys -- --lookup-name "Processor" ; name → index
```

### Alert on a single counter

```
check_pdh "counter=\\PhysicalDisk(_Total)\\Avg. Disk Queue Length" \
  "warn=value > 2" "crit=value > 5"
```

### Force min/max bounds for graphing

PDH doesn't tell NSClient++ what range a counter lives in. For percentages or known-bounded
counters, declare them yourself so graphing systems can auto-scale:

```
check_pdh "counter=\\Processor(_Total)\\% Processor Time" \
  "perf-config=*(minimum:0;maximum:100)"
```

See [Checks In Depth: Performance Data](../concepts/checks.md#8-performance-data) for the full
`perf-config` reference.

---

## Predefined Counters (averages over time)

A check that fires once a minute returns one *instantaneous* sample. For a noisy counter (CPU,
queue length, network rates) you usually want an **average over the last N seconds**.

Predefine the counter in the config file with `collection strategy = rrd`:

```ini
[/settings/system/windows/counters/disk_q]
collection strategy = rrd
counter             = \\PhysicalDisk(_Total)\\Avg. Disk Write Queue Length
```

Then check by *name*, asking for the average over the last 30 seconds:

```
check_pdh counter=disk_q time=30s
```

The named-counter form also sidesteps shell-escaping pain (the counter path is in the config file,
which is plain UTF-8) and lets you reuse the same definition from many checks.

### When to predefine vs. check by path

| Use a named/predefined counter when…       | Use a direct path when…              |
|--------------------------------------------|--------------------------------------|
| You want averaging (`time=30s` etc.)       | One-off ad-hoc check                 |
| The path has localised / non-ASCII chars   | Pure ASCII counter name              |
| Many checks use the same counter           | Single use                           |
| You're hitting shell-quoting headaches     | Counter syntax is shell-friendly     |

---

## Common Gotchas

### Counters are localised

A counter named `\\Processor(_Total)\\% Processor Time` on an English server is
`\\Procesor(_Total)\\% czas procesora` on a Polish one. Two ways to handle this:

1. **English fallback names** — NSClient++ resolves English names against the local registry. This
   is the simplest fix and works with the path syntax above.
2. **Index-based lookup** — counters have stable numeric indices. Use `nscp sys -- --lookup-name`
   to find the index, then reference the counter by index.

If a localised counter passes through NRPE/check_nt and gets mangled, set the encoding to match
the upstream (typically UTF-8) under `[/settings/default]`.

### Counters sometimes get lost

The PDH name registry occasionally corrupts itself — counters that worked yesterday return
"counter not found" today. Restore from the system cache:

```
lodctr /R
```

(Run as Administrator. Microsoft KB:
[https://support.microsoft.com/kb/300956](https://support.microsoft.com/kb/300956).)

### Rate counters always read 0

Counters whose unit is per-second (e.g. `\\PhysicalDisk\\Disk Reads/sec`) require two samples —
one read returns nothing meaningful. Add the `averages` flag to take two samples one second apart:

```
check_pdh "counter=\\PhysicalDisk(_Total)\\Disk Reads/sec" averages
```

### Wrong value range or capping

Some counters are capped (e.g. `% Processor Time` won't exceed 100) or scaled internally. To
disable capping/scaling, use the `flags` argument:

```
check_pdh "counter=\\Foo\\Bar" flags=nocap100,noscale
```

### Wrong datatype

Counters can be reported as `long`, `large` (64-bit), or `double`. NSClient++ defaults to `large`;
if a counter returns the wrong magnitude, override with `type=double` (or `long`).

---

## Via NRPE

```
check_nrpe -H <agent-ip> -c check_pdh -a "counter=\\Processor(_Total)\\% Processor Time" "warn=value > 80" "crit=value > 95"
```

If the counter path fails through NRPE due to escaping or localisation, predefine the counter on
the agent (see above) and check by name:

```
check_nrpe -H <agent-ip> -c check_pdh -a counter=disk_q time=30s
```

---

## Next Steps

- [Windows Server Health](windows-server-health.md) — combine PDH counters with CPU/memory/disk
  baselines
- [Checks In Depth: Performance Data](../concepts/checks.md#8-performance-data) — customise
  perfdata, force min/max bounds for graphing
- [Checks In Depth: Filters](../concepts/checks.md#5-filters-choosing-what-to-check) — write
  filter and threshold expressions
- [FAQ: Failed to open performance counters](../faq.md#13-failed-to-open-performance-counters) —
  rebuilding a broken PDH registry
- [Reference: CheckSystem](../reference/windows/CheckSystem.md) — full `check_pdh` command
  reference
