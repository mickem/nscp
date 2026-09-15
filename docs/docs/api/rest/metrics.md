# Metrics

NSClient++ exposes the metrics gathered by the running modules
(`CheckSystem`, `CheckDisk`, …) through three endpoints:

* [List metrics](#list-metrics) — `/api/v2/metrics` (JSON), with
  [`?meta=1`](#described-metrics) for what each metric means
* [OpenMetrics](#openmetrics) — `/api/v2/openmetrics` (text exposition)
* [Legacy /metrics](#legacy-metrics) — root `/metrics` (nested JSON)

The current API (`/api/v2`) only exposes metrics on `/api/v2`. There is no
`/api/v1/metrics` route — `/api/v1` clients should use the legacy `/metrics`
endpoint described below.

Modules that submit metrics push them through the core's metrics bus. The
WEBServer caches the latest snapshot; reads are non-blocking and serve the
last received data.

The two `/api/v2` endpoints are gated by their own grants, `metrics.list` and
`openmetrics.list`. The bundled [`metrics`
role](../../setup/web-interface.md#built-in-roles) holds both and nothing
else, which is what a scraper wants; `monitoring` holds them alongside
`queries.execute`.

## List metrics

Returns a flat dictionary mapping a dotted path (e.g.
`system.cpu.total.5m`) to a numeric or string value. This is the form used
by the bundled web UI.

| Key       | Value             |
|-----------|-------------------|
| Verb      | GET               |
| Address   | /api/v2/metrics   |
| Privilege | metrics.list      |

### Request

```
GET /api/v2/metrics
```

### Response

```json
{
    "system.cpu.total.5m": 12,
    "system.cpu.total.1m": 8,
    "system.cpu.total.5s": 6,
    "system.mem.physical.percent": 73,
    "system.mem.committed.percent": 81,
    "system.uptime": 36370
}
```

Numeric values that are integral are returned as JSON integers; values with
a fractional part are returned as JSON numbers. String-valued metrics
(rare, mostly version strings) are returned as JSON strings.

### Example

```
curl -s -k -u admin https://localhost:8443/api/v2/metrics | python -m json.tool
```

<a id="described-metrics"></a>
### Describing the metrics (`?meta=1`)

The flat map says what a metric reads, not what it means. `?meta=1` answers
with the same keys and values plus the metadata the
[OpenMetrics exposition](#metadata) is built from — the help text, the unit,
the type and the labels — so a dashboard can print `12 592 123 904 bytes`
rather than a bare number, from one request.

```
GET /api/v2/metrics?meta=1
```

`?meta=true` and `?meta=yes` mean the same thing. Anything else, `?meta=0`
included, is the plain flat map above: the described document is a different
shape, so it is opt-in and nothing that reads `/api/v2/metrics` today changes.

```json
{
    "metrics": {
        "system.mem.physical.used": 5123456789,
        "system.cpu.core 0.idle": 93,
        "system.uptime.uptime": "1d 12:30",
        "workers.jobs": 1847
    },
    "metadata": {
        "system.mem.physical.used": {
            "type": "gauge",
            "help": "Physical memory in use",
            "unit": "bytes"
        },
        "system.cpu.core 0.idle": {
            "type": "gauge",
            "help": "Share of CPU time spent idle",
            "unit": "percent",
            "labels": { "core": "0" }
        },
        "system.uptime.uptime": {
            "type": "info",
            "help": "Uptime as a human readable string"
        },
        "workers.jobs": {
            "type": "counter",
            "help": "Scheduled jobs the agent has started since it was started"
        }
    }
}
```

`metrics` is byte for byte what the endpoint returns without `meta`, and
`metadata` is keyed by the same keys. Every entry has a `type` — one of
`gauge`, `counter`, `unknown`, `info`, `summary` or `histogram`, as
[Types](#types) describes them; `help`, `unit` and `labels` appear only where
the producing module declared them, so a metric published through the bare
`add_metric()` shorthand (an out-of-tree module, a Python script returning a
plain number) carries its type and nothing it never said. `help` falls back to
the bundle's description exactly as the exposition's `# HELP` does.

The keys are the flat keys, unchanged: the metadata never renames anything.
The OpenMetrics *family* name is derived from the key and the unit and is not
repeated here — see [Metric names](#metric-names) for the mapping.

```
curl -s -k -u admin 'https://localhost:8443/api/v2/metrics?meta=1' | python -m json.tool
```

## OpenMetrics

Returns the same snapshot in
[OpenMetrics](https://openmetrics.io/) text exposition format, suitable for
Prometheus scraping. Every metric carries a description, a type and, where the
value is measured in something, a unit; string-valued metrics become labels of
their section's `_info` family. A metric measured once per core, NIC, drive or
process is one family carrying a [label](#labels) rather than one family per
instance.

| Key       | Value                |
|-----------|----------------------|
| Verb      | GET                  |
| Address   | /api/v2/openmetrics  |
| Privilege | openmetrics.list     |

### Request

```
GET /api/v2/openmetrics
```

### Response

```
# HELP system_mem_physical_total_bytes Physical memory fitted in the machine
# TYPE system_mem_physical_total_bytes gauge
# UNIT system_mem_physical_total_bytes bytes
system_mem_physical_total_bytes 17175158784
# HELP system_mem_physical_percent Share of physical memory still available
# TYPE system_mem_physical_percent gauge
# UNIT system_mem_physical_percent percent
system_mem_physical_percent 73
# HELP system_cpu_idle_percent Share of CPU time spent idle
# TYPE system_cpu_idle_percent gauge
# UNIT system_cpu_idle_percent percent
system_cpu_idle_percent{core="0"} 95
system_cpu_idle_percent{core="1"} 91
system_cpu_idle_percent{core="total"} 93
# HELP workers_jobs Scheduled jobs the agent has started since it was started
# TYPE workers_jobs counter
workers_jobs_total 1847
# TYPE system_uptime info
system_uptime_info{uptime="1d 12:30",boot="2026-09-13 01:15"} 1
# EOF
```

Every family carries a `# TYPE` line and the body ends with the `# EOF`
terminator OpenMetrics 1.0 requires, so a strict parser accepts the document
as it stands.

### Metadata

Every built-in metric declares what it is. A module hands the agent a
description, a unit and a type along with the value, and the exposition turns
them into the three metadata lines:

| Declared by the module | Emitted as                            |
|------------------------|---------------------------------------|
| description            | `# HELP <name> <description>`          |
| unit                   | `# UNIT <name> <unit>`, and the family name is made to end in `_<unit>` |
| type                   | `# TYPE <name> <type>`                 |

A metric that declares no description of its own inherits its section's, which
is how the per-core and per-NIC families are described without repeating the
same sentence for every instance.

Units are only declared where the value really is measured in something:
`bytes`, `seconds`, `percent`, `celsius`, `milliseconds`, `mhz`. A per-second
rate (`system.network.eth0.received`, `disk.io.sda.read_bytes_per_sec`)
declares none — the sample is a rate, and a name ending in `_bytes` would say
otherwise. Neither does a plain count.

Because OpenMetrics requires the name of a family that declares a unit to end
with that unit, declaring one can rename the family:
`system_mem_physical_total` became `system_mem_physical_total_bytes`. A key
that already ends in its unit keeps the name it had, which covers every `.%`
key (`system_mem_physical_percent`) and the clock frequencies
(`system_cpu_frequency_core_0_current_mhz`).

### Types

| Protobuf value    | Type        | Sample                          | Used for |
|-------------------|-------------|---------------------------------|----------|
| `gauge_value`     | `gauge`     | `name{…} v`                     | anything that can go down again — the great majority |
| `counter_value`   | `counter`   | `name_total{…} v`               | a count that only grows while the agent runs: jobs run, errors seen, `times_seen` |
| `untyped_value`   | `unknown`   | `name{…} v`                     | a number whose direction is genuinely unknown |
| `string_value`    | `info`      | `<section>_info{key="value",…} 1` | uptime, boot time, MAC address, power source |
| `summary_value`   | `summary`   | `name{quantile="…"}`, `name_sum`, `name_count` | nothing yet |
| `histogram_value` | `histogram` | `name_bucket{le="…"}`, `name_sum`, `name_count` | nothing yet |

The strings of one section fold into a single always-1 series carrying them as
labels — the `node_uname_info` shape — so `system.uptime.uptime` and
`system.uptime.boot` scrape as one `system_uptime_info` sample. They used to be
dropped entirely.

A metric that declares nothing at all still renders, as an anonymous gauge
with no `# HELP` and no `# UNIT`. That is what a Python script's plain number
and an out-of-tree module's `add_metric` produce.

### Metric names

The dotted path used in the JSON form is rewritten to the OpenMetrics name
grammar (`[a-zA-Z_][a-zA-Z0-9_]*`), deterministically:

| Rule                                            | JSON key                      | Metric name                    |
|-------------------------------------------------|-------------------------------|--------------------------------|
| `%` becomes the word                            | `system.mem.physical.%`       | `system_mem_physical_percent`  |
| anything else outside the grammar becomes `_`   | `system.cpu.core 0.idle`      | `system_cpu_core_0_idle`       |
| a run of separators collapses to one            | `disk.free.C:.total`          | `disk_free_C_total`            |
| a name that would not start with a letter borrows `metric_` | `5m_load`   | `metric_5m_load`               |

Colons are rewritten too: they are legal in the grammar but reserved for
user-defined recording rules, so an exporter must not emit them. A leading
underscore is reserved the same way, which is why a name that would not begin
with a letter borrows a `metric_` prefix rather than a bare `_`.

The mapping is lossy, so two different JSON keys can want the same metric
name. When that happens the first metric of the snapshot keeps the name and the
others are dropped - emitting both would mean the same series twice, which
costs the scraper the whole body rather than one metric. Each distinct
collision is logged once (and again after a settings reload), naming the metric
that was dropped and the name it collided on.

Values keep their full precision: an integral value is written out in full
(`17175158784`, not `1.7175e+10`), so a sample equals the number
`/api/v2/metrics` reports for the same key.

### Labels

A metric measured once per core, NIC, drive, thermal zone, battery or process
is one family with a label, not one family per instance. The instance stays in
the JSON key exactly where it was; the label is additive.

```
# TYPE system_cpu_idle_percent gauge
system_cpu_idle_percent{core="0"} 95
system_cpu_idle_percent{core="1"} 91
system_cpu_idle_percent{core="total"} 93
```

so `sum by (core)` has something to group on, a Grafana variable has a label to
bind to, and the family names no longer depend on how many cores or NICs a
particular host has. The same metric is still `system.cpu.core_0.idle` on
`/api/v2/metrics`.

| Bundle                     | Label      | Value                                                        |
|----------------------------|------------|--------------------------------------------------------------|
| `system.cpu`               | `core`         | `0`, `1`, … and `total` for the aggregate                  |
| `system.cpu_frequency`     | `cpu`          | the sysfs core (`cpu0`, `total`) on Linux; the processor's `DeviceID` (`CPU0`, `CPU1`) on Windows, which is one per socket |
| `system.network`           | `nic`          | the interface as the OS names it — `eth0` on Linux, the adapter *description* on Windows (`Intel(R) Ethernet Connection I219-LM`), not the friendly `Ethernet 1` |
| `system.temperature`       | `zone`         | the thermal zone or sensor                                 |
| `system.battery`           | `battery`      | the battery (`BAT0`); absent for a single unnamed battery  |
| `system.process_history`   | `exe`          | the executable name                                        |
| `system.metrics`           | `pdh_instance` | the PDH instance, for a counter configured with instances  |
| `disk.io`                  | `disk`         | the device (`sda`, `_Total`)                               |
| `disk.free`                | `drive`        | the drive or mount point (`C:`, `/`)                       |

The Windows `nic` value is the adapter description because that is what the
metric key has always been, and a key may not move. The friendly connection
name is published beside it as the `NetConnectionID` label of the same
`system_network_info` series, so a dashboard can show one and group on the
other.

`pdh_instance` is deliberately not called `instance`: Prometheus attaches its
own `instance` label — the scrape target — to every sample, and under the
default `honor_labels: false` an exported `instance` is renamed
`exported_instance`. A query written against `instance` would match the host
instead of the counter instance and quietly return nothing.

`core="total"` is the all-cores aggregate, mirroring the `system.cpu.total.*`
JSON key. It means `sum by (core)` stays honest but `sum without (core)`
double-counts — exclude it explicitly:

```promql
sum without (core) (system_cpu_idle_percent{core!="total"})
```

The labels also decide which series of a section's `_info` family a string
metric lands on, so one NIC's link state, MAC address and speed share a line
and the next NIC's get their own:

```
# TYPE system_network info
system_network_info{nic="Intel(R) Ethernet Connection I219-LM",NetConnectionID="Ethernet 1",MACAddress="00:11:22:33:44:55"} 1
system_network_info{nic="Realtek PCIe GbE Family Controller",NetConnectionID="Ethernet 2",MACAddress="00:11:22:33:44:66"} 1
```

Label names follow the same grammar as metric names and are rewritten the same
way, which matters only for the operator-defined ones (a PDH instance, a Python
script's labels). Label values are free text and are escaped rather than
rewritten: `\`, `"` and a newline are the three characters spelled
differently, so a Windows volume reads `drive="\\Device\\HarddiskVolume1"`.
A label with an empty value is dropped, because `x=""` and an absent `x` are
the same series.

Two samples of one family that end up with the same label set are the same
series, so the first is kept and the rest dropped and logged, exactly as for
two keys colliding on one name.

A metric with no labels — most of `system.mem`, the uptime and scheduler
sections, anything published by a module that does not set dimensions —
renders from its key as before.

### Content type

The endpoint answers `application/openmetrics-text; version=1.0.0;
charset=utf-8` when the request's `Accept` header names that type, and
`text/plain; version=0.0.4; charset=utf-8` otherwise. Prometheus asks for the
first; anything that does not negotiate gets the second.

The two bodies are not quite the same document, because the two specifications
disagree about what the metadata lines of a counter name. OpenMetrics names the
*family*, whose sample then carries the `_total` suffix; the older Prometheus
text format has no families, so its metadata lines name the sample itself:

```text
# Accept: application/openmetrics-text;version=1.0.0
# TYPE workers_jobs counter
workers_jobs_total 1847

# Accept: anything else
# TYPE workers_jobs_total counter
workers_jobs_total 1847
```

The same applies to `info`, which does not exist in the older format at all
and is written there as a gauge valued 1. Everything else — every gauge, every
`# HELP`, `# UNIT` and `# EOF` line — is identical, and the sample lines are
identical in both. The agent renders both bodies from one snapshot and serves
whichever matches the `Content-Type` it answers with, so the body a client
gets always matches the format it was told it is reading.

### The legacy exposition

The endpoint used to emit `<name> <value>` lines with the JSON keys
pasted in verbatim (dots, spaces, `%` and colons included), no metadata, no
terminator, and values truncated to six significant digits. Set

```ini
[/settings/WEB/server]
openmetrics format = legacy
```

to get that body back byte for byte while a dashboard or recording rule built
on the old names is migrated. The setting is deprecated and will be removed in
a future release.

### Prometheus scrape config

```yaml
scrape_configs:
  - job_name: nsclient
    metrics_path: /api/v2/openmetrics
    scheme: https
    tls_config:
      insecure_skip_verify: true
    basic_auth:
      username: admin
      password: <api password>
    static_configs:
      - targets: ['nsclient1.localdomain:8443']
```

### Example

```
curl -s -k -u admin https://localhost:8443/api/v2/openmetrics
```

## Legacy /metrics

The legacy controller exposes a `/metrics` endpoint at the root (no
`/api/` prefix) that returns a nested-object form of the same data. This
endpoint pre-dates the `/api/v*` controllers and is preserved for backward
compatibility.

| Key       | Value     |
|-----------|-----------|
| Verb      | GET       |
| Address   | /metrics  |
| Privilege | legacy    |

### Request

```
GET /metrics
```

### Response

```json
{
    "system": {
        "cpu": {
            "total": {
                "5m": 12,
                "1m": 8,
                "5s": 6
            }
        },
        "mem": {
            "physical": { "percent": 73 },
            "committed": { "percent": 81 }
        },
        "uptime": 36370
    }
}
```

### Example

```
curl -s -k -u admin https://localhost:8443/metrics | python -m json.tool
```

New integrations should prefer `/api/v2/metrics` (flat keys) or
`/api/v2/openmetrics` (Prometheus); the legacy nested form is harder to
consume and is gated behind the broad `legacy` privilege rather than a
dedicated `metrics.*` grant.
