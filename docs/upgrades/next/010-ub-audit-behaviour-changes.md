---
icon: "🔒"
modules: [core, filters, CheckSystem, CheckHelpers, NSClientServer, CheckMKServer, PythonScript]
action: conditional
---
**Undefined-behaviour audit: a few inputs that used to misbehave are now
rejected, and two servers restart on reload.** The fixes behind the
[security notice](../security/notices.md#undefined-behaviour-audit-crash-and-memory-safety-fixes-across-the-agent)
change behaviour in these places; nothing to do unless you relied on one:

- `check_cpu time=0` is refused with an error (it divided by zero); the
  window must be at least one second.
- A threshold literal that does not fit a 64-bit integer (`used > 1.0e30T`)
  is reported as an error on the check instead of being silently clamped, and
  a time or size whose unit multiplier overflows (`time=5000000w`,
  `size=9999999999T`) is refused rather than wrapping.
- A module whose `loadModuleEx` fails (REST `modules/<name>/commands/load`,
  the `nscp test` `load` command, a zip plugin) is now removed from the plugin
  list instead of lingering half-loaded; it no longer answers `exec`.
- `NSClientServer` (check_nt) and `CheckMKServer` stop their listener before a
  settings reload re-reads the configuration and start it again afterwards,
  so a changed port or password takes effect on reload as it already did for
  `NRPEServer`. Connections in flight at the moment of the reload are dropped.
- A Python script can no longer unload the `PythonScript` module it runs in
  (`Core.unload_module("PythonScript")` returns `False`).
- An empty `on_start` entry in a zip plugin's `module.json` is logged and
  skipped.
