---
icon: "🔧"
modules: [CheckMKServer]
action: conditional
---
**CheckMKServer: one `script` key replaces the `scripts` section.** The Lua
script that renders the check_mk agent output is now named by a single key,
`script` in `/settings/check_mk/server` (default `default_check_mk.lua`).
A list was never useful: there is one agent callback, and when several scripts
registered `check_mk.server()` the last one loaded won. Nothing to do on a
default install.
The old `/settings/check_mk/server/scripts` section is deprecated but still
read: while it has entries they are loaded instead of `script`, and a warning
is logged at startup. Move the file name to the new key and remove the section:

```ini
[/settings/check_mk/server]
script = my_check_mk.lua
```
