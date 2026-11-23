---
title: "IcingaAgent: Icinga commands answered under their own names"
fixed_in: next
severity: "Informational (experimental module, not loaded by default)"
modules: [IcingaAgent]
action: none
---
The experimental `IcingaAgent` module now turns the ITL's Windows commands
(`disk-windows`, `nscp-local`, ...) and the aliases under
`/settings/Icinga/agent/commands` into queries. That opens nothing an
`ifw-api` command does not (see
[what a connected Icinga parent may run](#icingaagent-what-a-connected-icinga-parent-may-run)):
the permission policy is asked about the query that results
(`CheckDisk.check_drivesize`, not `disk-windows`), and the values taken from
the parent count as arguments under `allow arguments`, so with
`allow arguments = false` only an alias whose arguments are all its own still
runs. Note that `nscp-local` names its query in a variable, so it can run any
query the policy grants.

**What to do:** nothing beyond what the module already asks for: enable the
permission policy and grant the parent only the checks it monitors. Set
`translate commands = false` to turn the built-in translations off.
