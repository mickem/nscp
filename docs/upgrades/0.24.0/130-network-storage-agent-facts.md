---
icon: "🏷️"
modules: [CheckSystem, CheckDisk, core]
action: none
---
**Three more fact sets: the network interfaces, the volumes and the agent
itself.** Nothing to do. Like `os` and `hardware`, each collects nothing until
you turn it on.

```ini
; CheckSystem. On Linux: [/settings/system/unix/facts]
[/settings/system/windows/facts]
network.interfaces = true

[/settings/disk/facts]
storage.volumes = true

[/settings/facts]
agent = true
```

| Set | Produced by | Fields |
|---|---|---|
| `network.interfaces` | CheckSystem | per interface: `id`, `display_name` (Windows), `mac`, `status`, `speed_bps`, `addresses` |
| `storage.volumes` | CheckDisk | per volume: `id`, `device`, `filesystem`, `type`, `label`, `size_bytes` |
| `agent` | the core | `version`, `modules`, `enrolled` |

A record's `id` is the name the matching check already uses for that instance:
`storage.volumes` ids are what `check_drivesize` calls `drive`, and
`network.interfaces` ids are what `check_network` calls `name`.

These sets describe the host, not how busy it is. They carry no free space and
no traffic counters, which change every round and belong to the checks. A
remote volume is listed without a size, so a dead share cannot stall the round.
`enrolled` is yes or no, never which server or which identity.

`network.interfaces` and `storage.volumes` are re-read every facts round,
because addresses and mounts change while the agent runs. See
[Host Facts](../concepts/facts.md) for every set, its fields and its cost.
