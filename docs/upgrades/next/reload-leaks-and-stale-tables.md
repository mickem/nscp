---
icon: "🔧"
modules: [LUAScript, DotnetPlugins, CheckHelpers, SimpleFileWriter, CheckMKClient, CheckMKServer]
action: none
---
**A settings reload no longer piles up scripts, plugins, aliases and syntax.**
Nothing to do; the difference shows on agents that reload often (fleet-managed
hosts, operators saving settings from the web UI).

* `LUAScript` unloads the previous generation of scripts before loading them
  again; every reload used to leak the old scripts and their Lua states, and to
  register every command a second time.
* `DotnetPlugins` unloads the managed plugin instances from the previous load
  before loading the configured ones again, and a plugin removed from the
  `plugins` section is no longer loaded again on reload. Every reload used to
  add another copy of each plugin, and queries were answered by whichever copy
  came first. The commands and channels the previous instances registered are
  taken back too, so the commands of a plugin removed from `[plugins]` are no
  longer listed (they used to fail with `No .NET plugin loaded`).
* `CheckHelpers` rebuilds its alias table on reload and unregisters the aliases
  that were removed from `[/settings/check helpers/alias]`; a removed alias
  used to keep resolving until the service was restarted.
* `SimpleFileWriter` replaces its line syntax on reload instead of appending to
  it: after N reloads every line written carried N copies of the syntax. A
  changed `channel` now moves the subscription instead of adding a second
  one; both channels used to write to the file.
* `CheckMKClient` and `CheckMKServer` release the previous generation of their
  Lua scripts on reload, which now runs each script's unload hook on every
  reload rather than only at shutdown. The bundled `default_check_mk.lua`
  does nothing there; a custom script with an unload hook that assumes it
  runs once, at shutdown, should expect it to run on every reload.
