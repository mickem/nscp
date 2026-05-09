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
are skipped. The output does **not** currently include `# HELP` or
`# TYPE` comments.

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
system_cpu_total_5m 12
system_cpu_total_1m 8
system_cpu_total_5s 6
system_mem_physical_percent 73
system_mem_committed_percent 81
system_uptime 36370
```

The dotted path used in the JSON form is rewritten to use underscores so
that the output is a valid OpenMetrics metric name.

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
