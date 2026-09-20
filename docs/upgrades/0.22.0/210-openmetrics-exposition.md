---
icon: "💥 📊"
modules: [WEBServer, CheckSystem, CheckSystemUnix, CheckDisk, Scheduler, PythonScript, GraphiteClient, CollectdClient, ElasticClient, core]
action: conditional
---
**`/api/v2/openmetrics` now serves a conformant, self-describing OpenMetrics
document, and every metric name changes.** Only affects scrapes of
`/api/v2/openmetrics`. The JSON endpoints (`/api/v2/metrics`, `/metrics`), the
web UI dashboard, Graphite, collectd and Python `submit_metrics` report the
same keys and the same values as before — the metric *key* is untouched and
stays authoritative for all of them.

The endpoint used to paste the JSON keys into the exposition verbatim, so names
carried `.`, `%`, spaces and colons (`system_mem_commited.avail`,
`system_cpu_core 0.idle`, `disk_free_C:.total`), there was no `# TYPE` and no
`# EOF`, values were truncated to six significant digits — 16 GB of memory
scraped as `1.6554e+10` — no module declared what any of its readings meant,
monotonic counts were typed as gauges so `rate()` was unsafe on them, and
string-valued metrics (uptime, boot time, MAC address, power source) were
dropped entirely. A strict parser rejected the body, and the scenario page told
you to repair the names with `metric_relabel_configs`.

Three things changed, and each of them renames families.

**1. Names are rewritten to the OpenMetrics grammar.**

| JSON key                  | Metric name                   |
|---------------------------|-------------------------------|
| `system.mem.physical.%`   | `system_mem_physical_percent` |
| `system.cpu.core 0.idle`  | `system_cpu_core_0_idle`      |
| `disk.free.C:.total`      | `disk_free_C_total`           |

`%` becomes the word `percent`, everything else outside `[a-zA-Z0-9_]` becomes
`_`, runs collapse to one, and a name that would not start with a letter
borrows a `metric_` prefix (a leading underscore is reserved). Every family
carries a `# TYPE` line, the body ends with `# EOF`, values keep their full
precision, and the response is typed
`application/openmetrics-text; version=1.0.0` when the scraper asks for it.

**2. Every metric carries a description, a type and a unit**, and declaring a
unit renames its family again:

* **`# HELP` on every built-in family**, and `# UNIT` wherever the value is
  measured in something.
* **Counters are typed as counters** — `workers.{jobs,submitted,errors}`,
  `scheduler.{jobs,submitted,errors}`,
  `system.process_history.<exe>.times_seen` and the real-time filter counts.
  Their sample carries the `_total` suffix the specification reserves for them.
* **Strings come back as an `_info` family**, the `node_uname_info` shape:
  `system_uptime_info{uptime="1d 12:30",boot="2026-09-13 01:15"} 1`.

A family that declares a unit has to end in it, so the name the key alone would
give gains a suffix:

| Name from the key alone     | Name it is served under           |
|-----------------------------|-----------------------------------|
| `system_mem_physical_total` | `system_mem_physical_total_bytes` |
| `system_cpu_total_idle`     | `system_cpu_total_idle_percent`   |
| `system_uptime_ticks_raw`   | `system_uptime_ticks_raw_seconds` |
| `disk_free_C_total`         | `disk_free_C_total_bytes`         |
| `workers_jobs`              | `workers_jobs_total`              |

A name that already ends in its unit keeps it, which covers every `.%` key
(`system_mem_physical_percent`) and the clock frequencies. A per-second rate
declares no unit and gains no suffix either (`system_network_eth0_received`,
`disk_io_sda_read_bytes_per_sec`).

**3. Per-instance metrics are one family with a label**, so every per-core,
per-NIC, per-drive and per-process family name changes again. Rewriting the key
alone would leave one family per core, which is what the endpoint has always
served: the family names then depend on how many cores a host has, `sum by
(core)` has nothing to group on, and a Grafana variable has no label to bind to.

```text
# one family per core, as a name-only rewrite would leave it
system_cpu_core_0_idle_percent 93
system_cpu_core_1_idle_percent 91
system_cpu_total_idle_percent 95

# what is served
# TYPE system_cpu_idle_percent gauge
system_cpu_idle_percent{core="0"} 93
system_cpu_idle_percent{core="1"} 91
system_cpu_idle_percent{core="total"} 95
```

The same move applies to every producer with an instance in its key:

| Bundle                   | Label          | Value                                                                 |
|--------------------------|----------------|-----------------------------------------------------------------------|
| `system.cpu`             | `core`         | `0`, `1`, … and `total` for the aggregate                             |
| `system.cpu_frequency`   | `cpu`          | the sysfs core on Linux; the processor `DeviceID` (`CPU0`) on Windows  |
| `system.network`         | `nic`          | the interface as the OS names it — the adapter description on Windows  |
| `system.temperature`     | `zone`         | the thermal zone or sensor                                            |
| `system.battery`         | `battery`      | the battery; absent for a single unnamed battery                      |
| `system.process_history` | `exe`          | the executable name                                                   |
| `system.metrics`         | `pdh_instance` | the PDH instance, for a counter configured with instances             |
| `disk.io`                | `disk`         | the device                                                            |
| `disk.free`              | `drive`        | the drive or mount point                                              |

`pdh_instance` rather than `instance`: Prometheus attaches its own `instance`
label (the scrape target) to every sample, and under the default
`honor_labels: false` an exported `instance` is renamed `exported_instance`, so
a query against `instance` would match the host rather than the counter.

The `_info` families gain the same labels, which is what splits them per
instance: one NIC's link state, MAC address and speed now share a series keyed
by `nic="…"`, instead of every NIC's strings piling onto one series under label
names like `eth0_status`. Windows and Linux spell a CPU core differently in the
JSON key (`core 0` and `core_0`); neither spelling reaches the label, which is
the bare `0` on both, so one query works across a mixed fleet.

What to do:

* **Drop any `metric_relabel_configs` block that rewrote dots to underscores.**
  The agent does exactly that itself now, so the rule no longer matches.
* **Update dashboards, recording rules and alerts** that name the old series.
  Every name moves at least once, and a per-instance one turns into a label:
  `system_cpu_core 0.idle` becomes `system_cpu_idle_percent{core="0"}`,
  `disk_free_C:.total` becomes `disk_free_total_bytes{drive="C:"}` and
  `workers_jobs` becomes `workers_jobs_total`. Most of them get shorter, and a
  query that used to enumerate instances can now aggregate.
* **Exclude `core="total"` from anything that aggregates over cores.** It is
  the all-cores aggregate, mirroring the `system.cpu.total.*` JSON key, so
  `sum without (core) (system_cpu_idle_percent)` double-counts. Write
  `system_cpu_idle_percent{core!="total"}` instead.
* **Check what you are taking a `rate()` of.** Only the counters listed above
  are monotonic. `system_network_<nic>_total` is a per-second rate and
  `system_os_updates_count` is how many updates are pending right now; both go
  down again, and a `rate()` of either was always nonsense — it is just visible
  now that the honest counters are labelled as such.
* If dashboards cannot be updated before the upgrade, set

    ```ini
    [/settings/WEB/server]
    openmetrics format = legacy
    ```

    which reproduces the pre-0.21 body byte for byte, with no metadata at all.
    It is deprecated and **will be removed in a future release**, so use it as
    a migration window rather than a setting to leave in place.

Two keys can now want the same metric name (`mem.used.%` and `mem.used percent`
both render as `mem_used_percent`). The first metric of the snapshot keeps the
name and the rest are dropped — emitting both would be the same series twice,
which costs the scraper the whole body rather than one metric. Each distinct
collision is logged once (again after a settings reload) naming the metric that
was dropped and the name it collided on. Nothing shipped produces such a pair;
a predefined PDH counter or a Python script can.

Smaller changes that come with it:

* **Windows only: `system.mem.page.%` and `system.mem.physical.%` report
  different numbers now, because they were reporting the wrong thing.** Both
  divided the *commit charge* by the commit limit while checking their own
  total for zero, so they published the commit figure under a page-file and a
  physical-memory name — and divided by zero on a machine that reported a page
  file or physical memory but no commit limit. Each reads its own numbers now.
  The JSON keys and the metric names are unchanged; only the values are, and
  they were wrong before. An alert threshold tuned against the old reading
  needs re-checking.

* **The two negotiated bodies are no longer identical.** The endpoint already
  answered `application/openmetrics-text; version=1.0.0` or
  `text/plain; version=0.0.4` depending on the request's `Accept` header. The
  two specifications disagree about what a counter's metadata lines name — the
  family in OpenMetrics (`# TYPE workers_jobs counter`, sample
  `workers_jobs_total`), the sample in the older format
  (`# TYPE workers_jobs_total counter`) — so the agent renders both and serves
  the one matching the type it answers with. Sample lines are identical in
  both. Nothing to do; a scraper that negotiates gets the richer form as it
  always did.

* **Predefined PDH counters can describe themselves.**
  `[/settings/system/windows/counters/<name>]` takes two new optional keys,
  `help` and `unit`, which become the `# HELP` and `# UNIT` lines of that
  counter's metric. `help` defaults to the counter path. Nothing else reads
  them, and a counter that sets neither behaves exactly as before.

* **Python `fetch_metrics` accepts a dict per value.**
  `{"value": 42, "help": "…", "unit": "bytes", "type": "counter"}` in place of
  a bare number, and `"labels": {"queue": "inbound"}` to label a metric. Plain
  numbers and strings keep meaning exactly what they did, and an out-of-tree
  C++ module calling `nscapi::metrics::add_metric()` is unaffected.

* **`/api/v2/metrics?meta=1` serves the same keys and values with their help
  text, unit, type and labels** under a `metadata` object, for a dashboard that
  wants to print a unit rather than a bare number. Without `meta` the endpoint
  is byte for byte what it was.

* **GraphiteClient can send the labels as carbon tags.** `metric tags = true`
  on a target appends `;core=0` to the metric path, which is otherwise
  unchanged. Off by default: a carbon older than 1.1 has no tag support and
  stores `path;core=0` as the metric name.

* **CollectdClient mappings can read the labels and the types.** A variable set
  to `label:core` expands to every distinct value of that label, instead of a
  regular expression over the flat keys that has to know whether the platform
  spells a core `core 0` or `core_0`; and a metric expression spelled `auto:`
  sends whatever the producing module declared a counter as a collectd DERIVE
  and everything else as a GAUGE. The built-in default mappings are unchanged.
  Note that a label includes the aggregates the exposition labels — `label:core`
  yields `total` alongside `0`, `1`, … — so a template built from it can name a
  metric that does not exist, which is skipped rather than sent.

* **A collectd value list naming a metric the snapshot does not carry is no
  longer sent as a zero.** The value expression resolved a missing key to an
  empty string and forwarded the `0` that parsed out of it, so a mapping naming
  a metric this platform or configuration never produces reported a
  measurement nobody took — which is what the platform-specific default
  mappings exist to avoid. Such a value list is now skipped, in whole: a
  collectd value list is positional, so dropping one value of several would
  have the receiver read the next metric's number under this one's type. A
  `derive:` expression also splits on `,` like `gauge:` always did, instead of
  looking the whole `a,b` string up as one key and sending a single zero. Only
  hand-written mappings are affected, and only where they were reporting
  zeroes.

See the [REST metrics reference](../api/rest/metrics.md#openmetrics) and the
[Prometheus scenario](../scenarios/prometheus.md) for the full rules.
