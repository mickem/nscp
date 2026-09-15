---
icon: "💥 📊"
modules: [WEBServer, CheckSystem, CheckSystemUnix, CheckDisk, PythonScript]
action: conditional
---
**Per-instance metrics are now one OpenMetrics family with a label, so every
per-core, per-NIC, per-drive and per-process family name changes again.** Only
affects scrapes of `/api/v2/openmetrics`. The JSON endpoints
(`/api/v2/metrics`, `/metrics`), the web UI dashboard, Graphite, collectd and
Python `submit_metrics` are byte for byte unchanged — the metric *key* is
untouched and stays authoritative for all of them; the labels are additive.

A metric measured once per core used to become one family per core, so the
family names depended on how many cores a host had, `sum by (core)` had nothing
to group on, and a Grafana variable had no label to bind to:

```text
# before
system_cpu_core_0_idle 93
system_cpu_core_1_idle 91
system_cpu_total_idle 95

# after
# TYPE system_cpu_idle gauge
system_cpu_idle{core="0"} 93
system_cpu_idle{core="1"} 91
system_cpu_idle{core="total"} 95
```

The same move applies to every producer with an instance in its key:

| Bundle                   | Label      | Value                                                       |
|--------------------------|------------|-------------------------------------------------------------|
| `system.cpu`             | `core`     | `0`, `1`, … and `total` for the aggregate                    |
| `system.cpu_frequency`   | `cpu`      | the sysfs core on Linux, the WMI processor name on Windows   |
| `system.network`         | `nic`      | the interface as the OS names it                             |
| `system.temperature`     | `zone`     | the thermal zone or sensor                                   |
| `system.battery`         | `battery`  | the battery; absent for a single unnamed battery             |
| `system.process_history` | `exe`      | the executable name                                          |
| `system.metrics`         | `instance` | the PDH instance, for a counter configured with instances    |
| `disk.io`                | `disk`     | the device                                                   |
| `disk.free`              | `drive`    | the drive or mount point                                     |

What to do:

* **Update dashboards, recording rules and alerts** that name a per-instance
  series. `system_cpu_core_0_idle` becomes `system_cpu_idle{core="0"}`,
  `disk_free_C_total` becomes `disk_free_total{drive="C:"}`, and so on. Most
  of them get shorter, and a query that used to enumerate instances can now
  aggregate.
* **Exclude `core="total"` from anything that aggregates over cores.** It is
  the all-cores aggregate, mirroring the `system.cpu.total.*` JSON key, so
  `sum without (core) (system_cpu_idle)` double-counts. Write
  `system_cpu_idle{core!="total"}` instead.
* If that cannot happen before the upgrade,
  `openmetrics format = legacy` under `[/settings/WEB/server]` still
  reproduces the pre-0.21 body byte for byte. It is deprecated and **will be
  removed in a future release**.

Windows and Linux spell a CPU core differently in the JSON key (`core 0` and
`core_0`); neither spelling reaches the label, which is the bare `0` on both,
so one query works across a mixed fleet.

A Python `fetch_metrics` callback can return
`{"value": 42, "labels": {"queue": "inbound"}}` in place of a bare value to
label its own metrics. A scalar still means an unlabelled gauge, and an
out-of-tree C++ module calling `nscapi::metrics::add_metric()` is unaffected.

See [Labels](../api/rest/metrics.md#labels) for the full rules and the
[Prometheus scenario](../scenarios/prometheus.md) for example queries.
