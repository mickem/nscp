---
icon: "🆕 🔒"
modules: [core, WEBServer, CheckExternalScripts, PythonScript, LUAScript, CheckSystem]
action: conditional
---
**New: on-demand fact sets (script listings, the performance counter catalog), a `modules` fact set, and per-set grants.**
The facts API gained a second kind of set. An **on-demand set** is collected
when its path is read, kept for five seconds, and never part of the stored
document: it is not in `GET /api/v2/facts`, does not move `revision`, does not
count against `[/settings/facts] max size` and is never sent to a fleet
server. There is nothing to enable - a set is available while the module that
produces it is loaded.

| Path | Produced by | Contains |
|---|---|---|
| `scripts.external`, `scripts.python`, `scripts.lua` | CheckExternalScripts, PythonScript, LUAScript | the script files in each runtime's folder (and, for external scripts, the names they are registered under). The listing only - never a script's source or what it runs |
| `scripts` | whichever are loaded | all of the above as one object |
| `pdh.objects` | CheckSystem (Windows) | every performance object with its counters |
| `pdh.instances.<object>` | CheckSystem (Windows) | the instances one object has right now |

`GET /api/v2/facts/sets` lists the sets an agent has, and `nscp test` reads
one with `facts scripts.python`. In the web UI the **Facts** page has an
*On demand* card that loads a set when asked.

A new **scheduled** set, `modules`, lists the loaded modules with their
version, alias and whether `[/modules]` loads them at start. It is opt-in like
every stored set: `modules = true` under `[/settings/facts]`.

Nothing changes for an existing reader: `GET /api/v2/facts`, every existing
path and the `facts.get` grant answer exactly as before.

**Custom roles:** an on-demand set is read with a per-set grant,
`facts.get.<set>` (`facts.get.scripts`, `facts.get.pdh`) - `facts.get` alone is
refused with a 403 naming the grant. No built-in role other than `full` holds
one, so add it to a role that should read them. The same form works for a
stored set (`facts.get.os`), which gives a role part of the inventory without
the rest. A role granted `facts.*` already covers all of them.

`GET /api/v2/scripts/<runtime>` and `GET /api/v2/metadata/counters` now read
these sets instead of running the module's command line; their bodies, status
codes and grants (`scripts.lists.<runtime>`, `metadata.get.counters`) are
unchanged. See the [REST reference](../api/rest/facts.md#read-an-on-demand-set),
[Host Facts](../concepts/facts.md#on-demand-sets) and the
[security notice](../security/notices.md#facts-on-demand-sets-take-a-per-set-grant-and-a-facts-wildcard-role-reads-script-listings).
