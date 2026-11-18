---
icon: "🧪"
modules: [IcingaAgent]
action: none
---
**New experimental `IcingaAgent` module: NSClient++ as a native Icinga 2 agent.**
Nothing to do: the module is not loaded by default, and it does nothing until
`/settings/Icinga/agent` has a `parent host`. When configured, NSClient++
connects to an Icinga 2 master or satellite over the cluster protocol (port
5665), the way the PowerShell-only Icinga for Windows does: it requests a
certificate from the Icinga CA (immediately with a `ticket`, otherwise after
`icinga2 ca sign`), and runs the checks the master schedules on it with
`command_endpoint`. Check commands follow the `ifw-api` convention: import
`ifw-api` into the CheckCommand and the master sends the query name and
argument definitions; `allowed commands` limits which queries the parent may
run.
