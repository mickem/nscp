# Metrics

NSClient++ exposes the metrics gathered by the running modules
(`CheckSystem`, `CheckDisk`, …) through three endpoints:

* [List metrics](#list-metrics) — `/api/v2/metrics` (JSON)
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

## OpenMetrics

Returns the same snapshot in
[OpenMetrics](https://openmetrics.io/) text exposition format, suitable for
Prometheus scraping. Only gauge values are emitted; string-valued metrics
are skipped.

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
# TYPE system_cpu_total_5m gauge
system_cpu_total_5m 12
# TYPE system_cpu_total_1m gauge
system_cpu_total_1m 8
# TYPE system_mem_physical_percent gauge
system_mem_physical_percent 73
# TYPE system_mem_physical_total gauge
system_mem_physical_total 17175158784
# EOF
```

Every family carries a `# TYPE` line and the body ends with the `# EOF`
terminator OpenMetrics 1.0 requires, so a strict parser accepts the document
as it stands. `# HELP` and `# UNIT` are not emitted yet: no module declares a
description or a unit for its metrics.

### Metric names

The dotted path used in the JSON form is rewritten to the OpenMetrics name
grammar (`[a-zA-Z_][a-zA-Z0-9_]*`), deterministically:

| Rule                                            | JSON key                      | Metric name                    |
|-------------------------------------------------|-------------------------------|--------------------------------|
| `%` becomes the word                            | `system.mem.physical.%`       | `system_mem_physical_percent`  |
| anything else outside the grammar becomes `_`   | `system.cpu.core 0.idle`      | `system_cpu_core_0_idle`       |
| a run of separators collapses to one            | `disk.free.C:.total`          | `disk_free_C_total`            |
| a leading digit gets a `_` prefix               | `5m_load`                     | `_5m_load`                     |

Colons are rewritten too: they are legal in the grammar but reserved for
user-defined recording rules, so an exporter must not emit them.

The mapping is lossy, so two different JSON keys can want the same metric
name. When that happens the first metric of the snapshot keeps the name, the
others are dropped, and each dropped metric is logged with the name it
collided on - emitting both would mean the same series twice, which costs the
scraper the whole body rather than one metric.

Values keep their full precision: an integral value is written out in full
(`17175158784`, not `1.7175e+10`), so a sample equals the number
`/api/v2/metrics` reports for the same key.

### Content type

The endpoint answers `application/openmetrics-text; version=1.0.0;
charset=utf-8` when the request's `Accept` header names that type, and
`text/plain; version=0.0.4; charset=utf-8` otherwise. The body is the same
either way - the Prometheus text parser reads `# EOF` as an ordinary comment.

### The legacy exposition

Before 0.22 the endpoint emitted `<name> <value>` lines with the JSON keys
pasted in verbatim (dots, spaces, `%` and colons included), no metadata, no
terminator, and values truncated to six significant digits. Set

```ini
[/settings/WEB/server]
openmetrics format = legacy
```

to get that body back byte for byte while a dashboard or recording rule built
on the old names is migrated. The setting is deprecated and will be removed in
0.24.0.

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
