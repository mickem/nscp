---
icon: "🔧"
modules: [core]
action: conditional
---
**A fleet `timeout` of zero and a negative size are now refused.**
`[/settings/fleet] timeout = 0` used to become a zero-second deadline on
every request to the fleet server; it is now reported and the default of
`60s` stays in force, the same as a `poll interval` under one second. The size
settings `[/settings/facts] max size` and `max entries`, `metrics context
budget`, `max context size` and `diagnostics rows` under
`[/settings/core/failed checks]` took `-1` as the largest possible number,
which removed the limit; they now take digits only, and a signed or padded
value (`-1`, `+5`, ` 5`) is reported and the value in force is kept. Nothing to
do unless one of these keys is set that way: set the number you mean.
