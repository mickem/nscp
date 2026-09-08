---
title: "Undefined-behaviour audit: crash and memory-safety fixes across the agent"
fixed_in: next
severity: "Medium"
modules: [core, filters, CheckSystem, CheckHelpers, CheckTaskSched, CheckEventLog, CheckDisk, CheckLogFile, WEBServer, NSClientServer, CheckMKServer, PythonScript, LUAScript, NRPEServer, NSCAClient]
action: none
---
After #1499 the C++ tree was scanned for the same class of bug and roughly
seventy findings were fixed. Three could be reached from outside the agent: any
HTTP server it talks to could hang it with a malformed chunked response, an
empty `POST /console/exec` command could crash it (with the `console.exec`
grant), and `filter_perf sort=normal` could crash on the Nagios `U` marker from
an external script. The rest were memory errors on the default paths of common
Windows checks, races when a module is reloaded or unloaded, and unguarded
arithmetic in thresholds and unit suffixes.

None is known to have been exploited, and none is known to do more than crash or
hang the agent.

**What to do:** nothing beyond upgrading. A few defaults changed as a
consequence; see the
[upgrade note](../setup/upgrading.md#unreleased).
