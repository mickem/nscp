---
icon: "🔧"
modules: [core]
action: conditional
---
**The keys under `[/settings/core]` are now known to the configuration check.**
`settings maintenance interval`, `metrics interval`, `diagnostics interval` and
`settings maintenance threads` were registered under the names `string` and
`int`, so the agent did not recognise its own keys. The values were read all
along, so nothing about how the agent runs changes; what changes is what it
says about the configuration. A misspelled key in that section (`metric
interval`) used to be passed over and now gets a warning in the log at startup
and on every reload, with the key it resembles, and `nscp settings --validate`
exits with `1` for it. The same command now also knows `[/settings/core]`,
`[/settings/core/failed checks]` and `[/settings/facts]` without a running
agent; with `--load-all` it used to report those sections, correctly spelled,
as read by nothing. Two things to check: a deployment that gates on
`nscp settings --validate` may now fail on a typo it let through before - fix
the spelling the warning suggests. And a configuration that has a literal
`string` or `int` key under `[/settings/core]` (possible only if it was written
out with defaults from a running agent) now has that key listed as unused;
nothing ever read it, so delete it. `nscp settings --generate --add-defaults`
now writes `agent = false` under `[/settings/facts]`, which is the default, and
an empty `[/settings/core/failed checks]` section.
