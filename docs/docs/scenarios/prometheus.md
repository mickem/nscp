# Exposing Metrics to Prometheus

**Goal:** Let a Prometheus server scrape NSClient++'s built-in metrics (CPU,
memory, uptime, network, temperature, predefined performance counters, …) so
they appear alongside everything else in Grafana / Alertmanager.

This scenario is the **inverse** of the rest of the integration scenarios:
NRPE/NSCA/NRDP/Icinga 2 *send check results* to a monitoring server, but
Prometheus *scrapes raw metrics* on its own schedule. The two patterns are
complementary — you can run both on the same agent.

---

## How It Works

NSClient++'s `WEBServer` module exposes an OpenMetrics endpoint at:

```
GET https://<agent>:8443/api/v2/openmetrics
```

Authenticated requests return the current metrics one-per-line in OpenMetrics
text format. Prometheus is configured to scrape that URL on its usual
interval (15s, 30s, 1m, …).

```
Prometheus ──HTTP scrape──► NSClient++ (WEBServer /api/v2/openmetrics)
                                       │
                                       └── pulls metrics from CheckSystem,
                                           CheckDisk, predefined counters, …
```

Metrics are aggregated by `WEBServer` from whichever modules happen to be
loaded — load `CheckSystem` to get CPU/memory/uptime/network, load `CheckDisk`
to add drive metrics, and so on.

---

## Prerequisites

```ini
[/modules]
WEBServer   = enabled    ; serves the /api/v2/openmetrics endpoint
CheckSystem = enabled    ; provides CPU / memory / uptime / network metrics
CheckDisk   = enabled    ; (optional) drive metrics
```

You also need:

- A reachable IP/hostname and TCP `8443` open from the Prometheus server.
- Credentials for the scrape (see Step 2 below).

---

## Step 1 — Enable the Web Server

If you haven't already, run the helper:

```
nscp web install
```

This sets a password, opens `8443` from `127.0.0.1`, and writes the role
configuration. Restart the service:

```
nsclient service --restart
```

For the full setup (TLS certificate, custom port, allowed hosts), see
[Web Interface](../setup/web-interface.md) — the Prometheus endpoint shares
the same web server, so anything that page covers applies here too.

---

## Step 2 — Create a Scrape User

Reading the OpenMetrics endpoint requires the `openmetrics.list` grant. The
built-in `full` role has `*` (everything), so the `admin` user can scrape
without further configuration — but a dedicated user with only the metrics
grant is better practice.

```ini
[/settings/WEB/server/roles]
prometheus = openmetrics.list,login.get

[/settings/WEB/server/users/prometheus]
role     = prometheus
password = <strong-random-password>
```

Restart NSClient++ for the new role/user to take effect.

---

## Step 3 — Verify the Endpoint

From the agent (or anywhere allowed to reach it):

```
curl -k -u prometheus:<password> https://<agent>:8443/api/v2/openmetrics
```

Expected output is one metric per line, `<name> <value>`:

```text
system_mem_commited.avail 12592123904
system_mem_commited.total 17175158784
system_mem_commited.% 73
system_cpu_total.idle 95
system_cpu_total.total 5
system_uptime_ticks.raw 84135
system_network_eth0_bytes_in 1029384...
```

If you get HTTP 401, the credentials or role grant are wrong; if you get a
TLS error, see "TLS / self-signed certificate" below.

---

## Step 4 — Configure Prometheus

Add a scrape job to `prometheus.yml`:

```yaml
scrape_configs:
  - job_name: nsclient
    scrape_interval: 30s
    metrics_path: /api/v2/openmetrics
    scheme: https
    tls_config:
      # NSClient++ generates a self-signed cert by default. Either point
      # `ca_file` at the CA you used for `nscp web install --certificate`,
      # or set `insecure_skip_verify: true` for a quick start (not for
      # production).
      insecure_skip_verify: true
    basic_auth:
      username: prometheus
      password: <strong-random-password>
    static_configs:
      - targets:
          - win-server-01.example.com:8443
          - win-server-02.example.com:8443
```

Reload Prometheus and check **Status → Targets** — the job should go green
within one scrape interval.

---

## Available Metrics

The exact set depends on which modules are loaded. From `CheckSystem` alone
you get (Windows host):

| Bundle                  | Examples                                                                     |
|-------------------------|------------------------------------------------------------------------------|
| `system_mem_*`          | `commited.avail`, `physical.total`, `page.used`, `virtual.%`                 |
| `system_cpu_*`          | `total.idle`, `total.user`, `total.kernel`, plus per-core variants           |
| `system_uptime_*`       | `ticks.raw`, `boot.raw`                                                      |
| `system_network_<nic>_*`| `bytes_in`, `bytes_out`, packet counters                                     |
| `system_temperature_*`  | ACPI thermal zone temperatures                                               |
| `system_metrics_*`      | Predefined PDH counters (see [Performance Counters](counters.md))            |

Add `CheckDisk` and you also get `disk_*` and `disk.io_*` bundles.

For ad-hoc Windows performance counters, predefine them in
`[/settings/system/windows/counters/<name>]` (see
[Performance Counter (PDH) Monitoring](counters.md)) — they appear on the
endpoint automatically as `system_metrics_<name>`.

---

## Common Gotchas

### Metric names contain dots

NSClient++ emits names like `system_mem_commited.avail`. Strict OpenMetrics /
Prometheus identifiers should match `[a-zA-Z_:][a-zA-Z0-9_:]*`, which doesn't
include `.`. Recent Prometheus versions tolerate it; older ones may reject
the line. If your scrape drops metrics with dots, rewrite them with
`metric_relabel_configs`:

```yaml
    metric_relabel_configs:
      - source_labels: [__name__]
        regex: '(.*)\.(.*)'
        target_label: __name__
        replacement: '${1}_${2}'
```

### TLS / self-signed certificate

Out of the box `nscp web install` generates a self-signed certificate.
Production options:

- Point Prometheus' `ca_file` at your internal CA and replace the cert with
  one signed by it (the `nscp web install --certificate ...` flag, or the
  cert-management UI in the web interface).
- Or use `insecure_skip_verify: true` in `tls_config` — fast to set up but
  doesn't authenticate the agent. Acceptable on a private network you trust;
  not on the open internet.

### `allowed hosts` blocks the scrape

Without an explicit allow list, the WEBServer accepts only `127.0.0.1`. If
Prometheus runs on a different host, add it:

```ini
[/settings/default]
allowed hosts = 127.0.0.1, 10.0.0.0/24
```

Or per-module under `[/settings/WEB/server]`.

### No `# HELP` / `# TYPE` lines

The endpoint emits only `<name> <value>` pairs — no metadata. Most metrics
are gauges; tag them as such on the Prometheus side if you need explicit
type metadata, or use `metric_relabel_configs` to project them into separate
series.

### Strings are skipped

Some bundles include string-typed entries (e.g. `system_uptime_uptime` is the
human-readable "1d 12:30"). These don't appear on the OpenMetrics endpoint —
only numeric gauges do. Use the JSON `/api/v2/metrics` endpoint if you need
the strings.

---

## Next Steps

- [Web Interface](../setup/web-interface.md) — full WEBServer setup including
  TLS, port, and user/role management.
- [Performance Counter (PDH) Monitoring](counters.md) — predefine custom PDH
  counters; they appear on the OpenMetrics endpoint automatically.
- [REST API](../api/rest/index.md) — the same web server also exposes
  `/api/v2/queries`, `/api/v2/metrics` (JSON), logs, and module management.
- [Reference: WEBServer](../reference/generic/WEBServer.md) — every web
  server setting in detail.
