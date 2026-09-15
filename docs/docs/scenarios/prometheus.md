# Exposing Metrics to Prometheus

**Goal:** Let a Prometheus server scrape NSClient++'s built-in metrics (CPU,
memory, uptime, network, temperature, predefined performance counters, …) so
they appear alongside everything else in Grafana / Alertmanager.

This works the same on **Windows and Linux** — the same modules, endpoint,
user/role setup and scrape configuration apply on both. The platforms differ
only in which metric families they emit (see
[Available Metrics](#available-metrics) below).

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

Authenticated requests return the current metrics as an OpenMetrics text
exposition. Prometheus is configured to scrape that URL on its usual interval
(15s, 30s, 1m, …).

```mermaid
flowchart LR
    P[Prometheus] -->|HTTP scrape| W[NSClient++<br/>WEBServer<br/>/api/v2/openmetrics]
    W --- CS[CheckSystem]
    W --- CD[CheckDisk]
    W --- PC[Predefined counters]
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
without further configuration — but a dedicated user is better practice.

The WEB module ships a `metrics` role for exactly this
(`public,metrics.list,openmetrics.list,login.get`): it reads
`/api/v2/openmetrics` and `/api/v2/metrics` and can do nothing else — in
particular it cannot run checks.

```ini
[/settings/WEB/server/users/prometheus]
role     = metrics
password = <strong-random-password>
```

Or from the command line:

```commandline
$ nscp web add-user prometheus --role metrics --password "<strong-random-password>"
```

A monitoring server that both runs checks and scrapes can use `monitoring`
instead, which holds the two metrics grants as well.

Restart NSClient++ for the new user to take effect.

---

## Step 3 — Verify the Endpoint

From the agent (or anywhere allowed to reach it):

```
curl -k -u prometheus:<password> https://<agent>:8443/api/v2/openmetrics
```

Expected output is an OpenMetrics document: `# HELP`, `# TYPE` and where
applicable `# UNIT` per family, one `<name> <value>` sample per line, and a
closing `# EOF`. On a Windows host:

```text
# HELP system_mem_commited_avail_bytes Commit charge still available
# TYPE system_mem_commited_avail_bytes gauge
# UNIT system_mem_commited_avail_bytes bytes
system_mem_commited_avail_bytes 12592123904
# HELP system_mem_commited_percent Share of the commit limit still available
# TYPE system_mem_commited_percent gauge
# UNIT system_mem_commited_percent percent
system_mem_commited_percent 73
# HELP system_cpu_idle_percent Share of CPU time spent idle
# TYPE system_cpu_idle_percent gauge
# UNIT system_cpu_idle_percent percent
system_cpu_idle_percent{core="0"} 93
system_cpu_idle_percent{core="total"} 95
# TYPE system_uptime info
system_uptime_info{uptime="1d 12:30",boot="2026-09-13 01:15"} 1
# HELP workers_jobs Scheduled jobs the agent has started since it was started
# TYPE workers_jobs counter
workers_jobs_total 1847
# EOF
```

On a Linux host:

```text
# HELP system_cpu_idle_percent Share of CPU time spent idle
# TYPE system_cpu_idle_percent gauge
# UNIT system_cpu_idle_percent percent
system_cpu_idle_percent{core="0"} 96.4
system_cpu_idle_percent{core="total"} 97.8293
# HELP system_mem_physical_total_bytes Total physical memory
# TYPE system_mem_physical_total_bytes gauge
# UNIT system_mem_physical_total_bytes bytes
system_mem_physical_total_bytes 16554000000
# HELP system_network_received Bytes received per second
# TYPE system_network_received gauge
system_network_received{nic="eth0"} 343
# EOF
```

Metric names are rewritten to the OpenMetrics grammar: `.` and any other
character outside `[a-zA-Z0-9_]` becomes `_`, a run of them collapses to one,
`%` becomes the word `percent`, and a name that would not start with a letter
borrows a `metric_` prefix. A family that declares a unit is then made to end
in it, which is what OpenMetrics requires of one — hence
`system_mem_physical_total_bytes`. The mapping is deterministic, so the same
reading always lands on the same series. Anything measured per core, NIC,
drive or process is one family with a label rather than one family per
instance. See the [REST metrics
reference](../api/rest/metrics.md#metric-names) for the full table,
[Metadata](../api/rest/metrics.md#metadata) for the unit rules and
[Labels](../api/rest/metrics.md#labels) for the label each section carries.

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
          - linux-server-01.example.com:8443
```

Windows and Linux agents can share the same scrape job — the endpoint, port
and authentication are identical.

Reload Prometheus and check **Status → Targets** — the job should go green
within one scrape interval.

---

## Available Metrics

The exact set depends on which modules are loaded and on the platform.
Available on **both platforms** from `CheckSystem`:

| Family prefix              | Examples                                                                         |
|----------------------------|----------------------------------------------------------------------------------|
| `system_cpu_*`             | `system_cpu_idle_percent`, `..._user_*`, `..._kernel_*`, labelled by `core`      |
| `system_mem_*`             | families differ per platform — see below                                        |
| `system_uptime_*`          | `system_uptime_ticks_raw`, `system_uptime_boot_raw`                              |
| `system_network_*`         | `received`, `sent`, `total` (bytes/s), labelled by `nic`                          |
| `system_temperature_*`     | thermal sensors, labelled by `zone` (WMI/ACPI on Windows, sysfs on Linux)        |
| `system_battery_*`         | charge/health, labelled by `battery`, on machines that have one                   |
| `system_cpu_frequency_*`   | current/max clock, labelled by `cpu` (a socket on Windows), where exposed        |
| `system_process_history_*` | `times_seen` / `currently_running`, labelled by `exe` (opt-in, below)            |

Add `CheckDisk` (either platform) and you also get `disk_io_*` (throughput,
IOPS, queue length, busy time) labelled by `disk`, and `disk_free_*`
(total/free/used and percentages) labelled by `drive`.

The instance is a label, so one family covers every core, NIC, drive or
process on the host. That is what makes a Grafana variable and an aggregation
work:

```promql
# busiest core on each host
min by (instance) (system_cpu_idle_percent{core!="total"})

# total received bytes/s across every interface
sum by (instance) (system_network_received)

# every filesystem under 10% free
disk_free_free_pct < 10
```

`core="total"` is the all-cores aggregate, so exclude it from anything that
aggregates over cores (`sum without (core)` would double-count). The full
label-per-bundle table is in the [REST metrics
reference](../api/rest/metrics.md#labels).

`# HELP` says what each one is, so `curl`ing the endpoint (or Grafana's metric
browser) is enough to find out what a family means without coming back here.

Platform differences to be aware of:

- **Memory families** follow what the OS exposes: Windows publishes
  `commited` / `physical` / `page` / `virtual`, Linux publishes `physical` /
  `cached` / `swap`.
- **Per-core CPU naming**: Linux normalises core names to `core_0`, `core_1`,
  …; Windows names them `core 0` (with a space). Neither reaches the
  OpenMetrics endpoint, where both platforms scrape as
  `system_cpu_idle_percent{core="0"}`; the JSON endpoints still show the
  platform's own spelling.
- **PDH counter instances** carry a `pdh_instance` label, not `instance` —
  Prometheus uses `instance` for the scrape target and renames an exported one
  to `exported_instance`.
- **PDH counters** (`system_metrics_*`) are Windows-only: predefine them in
  `[/settings/system/windows/counters/<name>]` (see
  [Performance Counter (PDH) Monitoring](counters.md)) and they appear on the
  endpoint automatically. There is no Linux equivalent.
- **Process history** is opt-in on both platforms — set
  `process history = true` under `[/settings/system/windows]` or
  `[/settings/system/unix]` respectively.
- **Hardware metrics** (temperature, battery, CPU frequency) depend on what
  the host exposes: virtual machines and WSL typically publish none, which is
  normal.

---

## Common Gotchas

### Metric names changed

The endpoint used to emit the JSON keys verbatim, so names carried dots,
spaces and colons (`system_mem_commited.avail`, `system_cpu_core 0.idle`) and
the documented workaround was to rewrite them with `metric_relabel_configs`.
The agent does that itself now, by the rules above, so **drop any
`metric_relabel_configs` block that was rewriting dots** — it no longer matches
anything, and a rule that rewrote `(.*)\.(.*)` to `${1}_${2}` is exactly what
the agent already applied.

A dashboard, recording rule or alert written against the old names does need
updating. To buy time for that, put the old body back:

```ini
[/settings/WEB/server]
openmetrics format = legacy
```

This reproduces the old exposition byte for byte. It is deprecated and
will be removed in a future release, so treat it as a migration window rather
than a setting to leave in place.

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

### Counters, and what `rate()` is safe on

A metric is typed as a `counter` only where the value is monotonic for the
lifetime of the agent — `workers_jobs`, `scheduler_jobs`, `scheduler_errors`,
`system_process_history_<exe>_times_seen` and the real-time filter counts.
Those are the ones `rate()` and `increase()` are meaningful on, and their
sample carries the `_total` suffix the spec reserves for them.

Everything else is a gauge, including names that read like counts:
`system_network_<nic>_total` is a per-second rate, `system_os_updates_count` is
how many updates are pending right now, and both drop back down. Taking a
`rate()` of either produces nonsense at every dip.

`promtool check metrics` lints naming conventions as well as syntax, so it
still warns about a gauge whose name ends in `total` or `count` (a suffix the
spec reserves) and about the units that are not the spec's base ones —
`_milliseconds` where it would prefer seconds, `_mhz` where it would prefer
hertz. Those readings are genuinely in those units and the JSON endpoint
reports the same number, so the agent says what it measured rather than
rescaling it behind the reader's back.

### Strings arrive as an `_info` family

Some bundles include string-typed entries — `system.uptime.uptime` is the
human-readable "1d 12:30", `system.battery.power_source` is `ac` or `battery`.
They have no numeric sample, so each section folds its strings into one
always-1 series carrying them as labels, the same shape as node_exporter's
`node_uname_info`:

```text
# TYPE system_uptime info
system_uptime_info{uptime="1d 12:30",boot="2026-09-13 01:15"} 1
```

Query them with `system_uptime_info` and read the label, or use the JSON
`/api/v2/metrics` endpoint, which still reports each string under its own key.
Before the metadata work these were dropped from the exposition entirely.

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
