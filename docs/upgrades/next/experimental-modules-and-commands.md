---
icon: "🧪"
modules: [core, docs, CheckDisk, CheckDocker, CheckMSSQL, CheckMySQL, CheckNSCP, CheckNet, CheckSecurity, CheckSystem, CheckSystemUnix, CheckWindowsApps, IcingaClient, NSCANgClient, Scheduler]
action: none
---
**New modules and check commands are now marked *experimental*.** Nothing to do
on an upgrade: every check keeps working exactly as before. A module or check
command that is new enough that its options, filter keywords and output may
still change now says so — `nscp test` appends `(experimental)` to the name in
`queries`, `aliases`, `list` and `plugins` (and shows a `Status:` line in
`desc`), the web UI shows an *Experimental* chip on the module and query pages,
the REST API reports it as an `experimental` field on
[modules](../api/rest/modules.md), [queries](../api/rest/queries.md) and
[aliases](../api/rest/aliases.md), and the reference documentation renders a
marker in the command tables plus a note on the command itself.

Everything added in the last six months is marked: the recently added `check_*`
commands in `CheckDisk`, `CheckDocker`, `CheckNet`, `CheckNSCP`, `CheckSystem`,
`CheckSystemUnix` and `Scheduler`, plus the whole of the `CheckMSSQL`,
`CheckMySQL`, `CheckSecurity`, `CheckWindowsApps`, `IcingaClient` and
`NSCANgClient` modules.
The marker is not a warning that a check is broken — it is a statement about
stability: pin the options and syntax you depend on, and re-read the command's
reference page after an upgrade. Marks are removed as the commands settle.

If you build your own modules, declare it in `module.json` — `"experimental":
true` inside `"module"` for the whole module, or on a single command entry:

```json
"commands": {
    "check_new_thing": { "description": "…", "experimental": true }
}
```
