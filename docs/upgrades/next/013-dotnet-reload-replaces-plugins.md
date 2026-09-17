---
icon: "🔧"
modules: [DotnetPlugins, CheckHelpers, CommandClient]
action: none
---
**A few modules now replace on reload what they used to accumulate.** Nothing
to do; the previous behaviour was a leak or a duplicate in every case.

* `DotnetPlugins` unloads the managed plugins it has loaded before reading the
  configuration again. A reload used to create a second managed instance of
  every plugin, leave the first one running and register the same commands
  again, so which instance answered a command was a matter of ordering. A
  plugin removed from the configuration now actually goes away.
* `LUAScript` unloads the previous generation of scripts on reload instead of
  dropping it. Each generation owned its own Lua states, so a fleet-managed
  host reloading every few minutes accumulated them.
* Command aliases (`CheckHelpers`, `CheckExternalScripts`) and client target
  definitions (`NRPEClient`, `NSCAClient`, `NRDPClient`, `IcingaClient`,
  `GraphiteClient`, `SyslogClient`, `SMTPClient`, `CollectdClient`, `NSCPClient`,
  `NSCA-NG`, `CheckMKClient`) are rebuilt and swapped in as a unit. An alias or
  a target removed from the configuration now disappears on reload rather than
  surviving until a restart, a changed target address or password now takes
  effect on reload, and a request arriving mid-reload no longer sees an empty
  target table (`connect to : failed`).
* `SimpleFileWriter` rebuilds its line syntax on reload instead of appending to
  it. After N reloads every written line carried N copies of the syntax.
* `nscp test` refuses a second console instead of starting one on top of the
  first.
