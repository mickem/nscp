---
icon: "🔧"
modules: [LUAScript, DotnetPlugins, CheckHelpers, SimpleFileWriter]
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
  came first.
* `CheckHelpers` rebuilds its alias table on reload and unregisters the aliases
  that were removed from `[/settings/check helpers/alias]`; a removed alias
  used to keep resolving until the service was restarted.
* `SimpleFileWriter` replaces its line syntax on reload instead of appending to
  it: after N reloads every line written carried N copies of the syntax.
