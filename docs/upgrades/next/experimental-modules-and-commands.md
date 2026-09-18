---
icon: "🧪"
modules: [core, docs, CheckDisk, CheckDocker, CheckMSSQL, CheckMySQL, CheckNSCP, CheckNet, CheckSecurity, CheckWindowsApps, NSCANgClient, Scheduler]
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

The recently added `check_*` commands of `CheckDisk`, `CheckDocker`, `CheckNet`,
`CheckNSCP` and `Scheduler` are marked, as are the `CheckMSSQL`, `CheckMySQL`,
`CheckSecurity`, `CheckWindowsApps` and `NSCANgClient` modules in full.

The marker is not a warning that a check is broken — it is a statement about
stability: pin the options and syntax you depend on, and re-read the command's
reference page after an upgrade. Marks are removed as the commands settle.

If you build your own modules, declare it in `module.json` — `"experimental":
true` inside `"module"` for the whole module, which covers every command it
registers, or on a single command entry when only that check is new:

```json
"commands": {
    "check_new_thing": { "description": "…", "experimental": true }
}
```
