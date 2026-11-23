---
icon: "🧪 🔒"
modules: [IcingaAgent]
action: none
---
**`IcingaAgent` answers the ITL's Windows commands, and `nscp icinga-agent import` takes over an Icinga agent.**
Nothing to do: the module is experimental and not loaded by default. The
Windows check commands of the Icinga Template Library (`disk-windows`,
`load-windows`, `service-windows`, ..., and the `nscp-local` commands) are
now answered under their Icinga names, with the service's own thresholds
converted to the `-w` / `-c` of the matching query, so a host that ran the
Icinga 2 agent keeps its services unchanged. Your own Icinga commands can be
mapped to queries under `/settings/Icinga/agent/commands`, and
`translate commands = false` turns the built-in translations off.
`nscp icinga-agent import` copies that agent's certificate and parent so
NSClient++ connects as the same endpoint, and `--stop-icinga` stops the
agent it replaces: see
[Migrating from the Icinga 2 Windows Agent](../scenarios/migrating-from-icinga-agent.md).
The query that results is what the permission policy sees; see the
[security notice](../security/notices.md#icingaagent-icinga-commands-answered-under-their-own-names).
