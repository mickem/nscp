---
icon: "🧪 🔒"
modules: [IcingaAgent]
action: none
---
**New experimental `IcingaAgent` module: NSClient++ as a native Icinga 2 agent.**
Nothing to do: the module is not loaded by default, and it does nothing until
`/settings/Icinga/agent` has a `parent host` and a `parent endpoint`. When
configured, NSClient++ connects to an Icinga 2 master or satellite over the
cluster protocol (port 5665), the way the PowerShell-only Icinga for Windows
does: it requests a certificate from the Icinga CA (immediately with a
`ticket`, otherwise after `icinga2 ca sign`), and runs the checks the master
schedules on it with `command_endpoint`. Check commands follow the `ifw-api`
convention: import `ifw-api` into the CheckCommand and the master sends the
query name and argument definitions. Checks run as
`IcingaAgent:<parent endpoint>`, so the permission policy decides which ones
the parent may run. Install the master's `ca.crt` with the agent, or set
`parent fingerprint`, so the first connection is verified too: an unverified
one sends the ticket to whoever answers; see the
[security notice](../security/notices.md#icingaagent-what-a-connected-icinga-parent-may-run).
