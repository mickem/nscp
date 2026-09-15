---
icon: "💥 📊"
modules: [WEBServer, CheckSystem, CheckSystemUnix, CheckDisk, Scheduler, PythonScript, GraphiteClient, ElasticClient, core]
action: conditional
---
**Every metric now carries a description, a type and a unit, and declaring a
unit renames its family on `/api/v2/openmetrics`.** Only affects scrapes of
`/api/v2/openmetrics`; the JSON endpoints (`/api/v2/metrics`, `/metrics`), the
web UI dashboard, Graphite, collectd and Python `submit_metrics` report the
same keys and the same values as before.

The endpoint emitted a name, a value and a `# TYPE ... gauge` line and nothing
else: no module declared what any of its readings meant, monotonic counts were
typed as gauges so `rate()` was unsafe on them, and string-valued metrics
(uptime, boot time, MAC address, power source) were dropped from the exposition
entirely. Now:

* **`# HELP` on every built-in family**, and `# UNIT` wherever the value is
  measured in something.
* **Counters are typed as counters** — `workers.{jobs,submitted,errors}`,
  `scheduler.{jobs,submitted,errors}`,
  `system.process_history.<exe>.times_seen` and the real-time filter counts.
  Their sample carries the `_total` suffix the specification reserves for them.
* **Strings come back as an `_info` family**, the `node_uname_info` shape:
  `system_uptime_info{uptime="1d 12:30",boot="2026-09-13 01:15"} 1`.

What to do:

* **Update dashboards, recording rules and alerts that name a family which
  gained a unit suffix.** A family that declares a unit has to end in it, so
  for example:

    | Before                            | After                                    |
    |-----------------------------------|------------------------------------------|
    | `system_mem_physical_total`       | `system_mem_physical_total_bytes`        |
    | `system_cpu_total_idle`           | `system_cpu_total_idle_percent`          |
    | `system_uptime_ticks_raw`         | `system_uptime_ticks_raw_seconds`        |
    | `disk_free_C_total`               | `disk_free_C_total_bytes`                |
    | `workers_jobs`                    | `workers_jobs_total`                     |

    A name that already ended in its unit is unchanged, which covers every
    `.%` key (`system_mem_physical_percent`) and the clock frequencies. A
    per-second rate declares no unit and is unchanged too
    (`system_network_eth0_received`, `disk_io_sda_read_bytes_per_sec`).

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
    It is deprecated and **will be removed in a future release**.

Two smaller changes come with it:

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
  a bare number. Plain numbers and strings keep meaning exactly what they did.

See the [REST metrics reference](../api/rest/metrics.md#metadata) and the
[Prometheus scenario](../scenarios/prometheus.md) for the full rules.
