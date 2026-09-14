---
icon: "💥 📊"
modules: [WEBServer]
action: conditional
---
**`/api/v2/openmetrics` now serves a conformant OpenMetrics document, and every
metric name changes.** Only affects scrapes of `/api/v2/openmetrics`; the JSON
endpoints (`/api/v2/metrics`, `/metrics`), the web UI dashboard, Graphite,
collectd and Python `submit_metrics` are byte for byte unchanged.

The endpoint used to paste the JSON keys into the exposition verbatim, so names
carried `.`, `%`, spaces and colons (`system_mem_commited.avail`,
`system_cpu_core 0.idle`, `disk_free_C:.total`), there was no `# TYPE` and no
`# EOF`, and values were truncated to six significant digits — 16 GB of memory
scraped as `1.6554e+10`. A strict parser rejected the body, and the scenario
page told you to repair the names with `metric_relabel_configs`.

Names are now rewritten to the OpenMetrics grammar by the agent:

| JSON key                  | Metric name                   |
|---------------------------|-------------------------------|
| `system.mem.physical.%`   | `system_mem_physical_percent` |
| `system.cpu.core 0.idle`  | `system_cpu_core_0_idle`      |
| `disk.free.C:.total`      | `disk_free_C_total`           |

`%` becomes the word `percent`, everything else outside `[a-zA-Z0-9_]` becomes
`_`, runs collapse to one, and a leading digit gets a `_` prefix. Every family
carries a `# TYPE ... gauge` line, the body ends with `# EOF`, values keep
their full precision, and the response is typed
`application/openmetrics-text; version=1.0.0` when the scraper asks for it.

What to do:

* **Drop any `metric_relabel_configs` block that rewrote dots to underscores.**
  The agent does exactly that itself now, so the rule no longer matches.
* **Update dashboards, recording rules and alerts** that name the old series.
  Most names only lose their dot; `.%` endings become `_percent` and Windows
  per-core names lose their space.
* If that cannot happen before the upgrade, set

  ```ini
  [/settings/WEB/server]
  openmetrics format = legacy
  ```

  which reproduces the old body byte for byte. It is deprecated and **will be
  removed in 0.24.0**, so use it as a migration window rather than a setting to
  leave in place.

Two keys can now want the same metric name (`mem.used.%` and
`mem.used percent` both render as `mem_used_percent`). The first metric of the
snapshot keeps the name, the rest are dropped, and each drop is logged with the
name it collided on — emitting both would be the same series twice, which costs
the scraper the whole body rather than one metric. Nothing shipped produces
such a pair; a predefined PDH counter or a Python script can.

See the [REST metrics reference](../api/rest/metrics.md#openmetrics) and the
[Prometheus scenario](../scenarios/prometheus.md) for the full rules.
