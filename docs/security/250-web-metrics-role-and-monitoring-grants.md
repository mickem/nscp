---
title: "WEB: a metrics role, and a corrected metrics grant on the monitoring role"
fixed_in: next
severity: "Low"
modules: [WEBServer]
action: conditional
---
The bundled `monitoring` role listed `metrics.get` among its grants, but no
endpoint has ever required that privilege: `/api/v2/metrics` is gated by
`metrics.list` and `/api/v2/openmetrics` by `openmetrics.list`, and grants
match per dot-separated segment, so `metrics.get` matched neither. A
`monitoring` user could therefore not read metrics at all, and the documented
advice to use that role for Prometheus produced a 403 with nothing in the role
list to explain it.

`monitoring` now grants `metrics.list,openmetrics.list` in place of the dead
`metrics.get`, so on a **fresh install** a `monitoring` user can read the
metrics endpoints — access the role was always described as having, but which
it did not in fact confer. Existing configurations are not rewritten: the role
string already in `nsclient.ini` is left exactly as it is, so nothing widens
under an upgrade.

Because a scraper needs no ability to run checks, the module also ships a
`metrics` role (`public,metrics.list,openmetrics.list,login.get`) that reads
the two endpoints and nothing else. Prefer it over `monitoring` for Prometheus:
it does not carry `queries.execute`, which can run any registered command.

**What to do:** if you assign the built-in `monitoring` role and do not want
its users reading metrics, pin the grants you want explicitly in
`[/settings/WEB/server/roles]` rather than relying on the shipped default. If
you have a scrape user on `monitoring` (or a hand-written role) and want it
narrowed, move it to `metrics` — see
[Prometheus scraping](../scenarios/prometheus.md).
