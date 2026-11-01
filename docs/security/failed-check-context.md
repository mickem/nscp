---
title: "Failed checks can carry a metrics snapshot, and are reported to an enrolled fleet server"
fixed_in: next
severity: "Low"
modules: [core, WEBServer]
action: conditional
---
Two new ways the failed-check log's contents reach a reader:

* **A metrics snapshot per failure.** With `capture metrics = true` under
  `[/settings/core/failed checks]` (off by default), each failed check also
  keeps the agent's latest metrics snapshot: everything `/api/v2/metrics`
  serves, from CPU and memory to the string metrics some modules publish
  (host names, OS and version strings, interface names, addresses). It is
  served at `GET /api/v2/failures/<index>/context` under the existing
  `failures.list` grant. A caller holding `failures.list` can therefore read
  past metrics too, which `/api/v2/metrics` only serves to a caller with
  `metrics.list`.
* **A report to the fleet server.** An enrolled agent whose fleet server asks
  for them (`X-Failure-Context: accept` on its desired-state answers) sends it
  one failed check per `failure context interval` (default `1h`): the entry
  as `/api/v2/failures` renders it, the snapshot when one was captured, and a
  per-check count of the other failures since the last report. This is new
  data leaving the host: check output lines and performance data, who ran the
  check, the arguments when `record arguments` is on, and the whole snapshot.

The report travels only to the server the host is enrolled with, over the
same pinned mTLS channel as the rest of the fleet sync, and only when that
server asks. A server that does not send the header gets nothing. The
redactions are the log's: arguments are off by default, and a `password=` or
`token=` value is replaced by `***` in the arguments and in an echoed message.
A secret a check prints under another name is reported as the check printed
it, exactly as it already reaches the monitoring server.

**What to do:** nothing on a default install: capture is off, so a report
carries no metrics. Before turning `capture metrics` on, decide whether the
roles holding `failures.list` may read metrics. To keep failed checks off the
fleet server altogether, set `failure context = false` under
`[/settings/fleet]`. That key is read when the service starts.
