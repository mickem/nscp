---
icon: "🔒"
modules: [core, filters, CheckSystem, CheckHelpers, NSClientServer, PythonScript, CheckMKServer]
action: conditional
---
**Undefined-behaviour audit: some inputs that used to misbehave are now
rejected.** From the
[security notice](../security/notices.md#undefined-behaviour-audit-crash-and-memory-safety-fixes-across-the-agent);
nothing to do unless you relied on one of these:

- `check_cpu time=0` is an error — the window must be at least one second.
- A threshold or unit suffix that overflows 64 bits (`used > 1.0e30T`,
  `time=5000000w`) is reported as an error instead of silently wrapping.
- A module whose load fails is dropped from the plugin list instead of
  lingering half-loaded, and no longer answers `exec`.
- `NSClientServer` (check_nt) and `CheckMKServer` restart their listener on a
  settings reload, so a changed port or password takes effect as it already did
  for `NRPEServer`. Connections open at that moment are dropped.
- A Python script can no longer unload the `PythonScript` module it runs in.
