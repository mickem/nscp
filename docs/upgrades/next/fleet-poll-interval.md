---
icon: "🆕 🔧"
modules: [core]
action: none
---
**New: `poll interval`, `min poll interval` and `max poll interval` for the fleet sync.**
An enrolled agent polls the fleet server as often as the server tells it to.
Three settings under `[/settings/fleet]` now bound that from the host's side:
`min poll interval` (default `1s`) is the shortest interval the server may ask
for and `max poll interval` (default `1d`) the longest, with anything outside
them held to the bound; `poll interval` (default `60s`) is what is used until
the server's first answer names one. Set `min poll interval = 5m` on a host
whose link should not carry a poll every minute, or `max poll interval = 1h`
on one that must pick up a configuration change within the hour. The defaults
are the limits the agent already had, so nothing changes until one is set. A
`Retry-After` from the server is still slept as given. Read when the service
starts. See [Fleet management](fleet.md#step-6-living-with-it).
