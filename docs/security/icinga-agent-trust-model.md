---
title: "IcingaAgent: what a connected Icinga parent may run"
fixed_in: next
severity: "Informational (new module, not loaded by default)"
modules: [IcingaAgent]
action: conditional
---
The new experimental `IcingaAgent` module connects out to an Icinga 2 master
or satellite and runs the checks that parent schedules on it. A connected
parent can therefore run queries on the agent, with arguments, much as a
caller of the REST API or of NRPE with `allow arguments = true` can. The
module only ever runs queries: Icinga event commands and the master's ad-hoc
`execute-command` action are refused, so nothing reaches the exec surface.

What stands between a parent and the agent's checks:

- **Mutual TLS against the Icinga CA.** The agent verifies the parent against
  the CA the parent issued when it signed the agent's certificate. The very
  first connection, before that CA exists on the agent, cannot be verified
  (the same trust on first use as `icinga2 node setup` without
  `--trustedcert`); the agent runs no checks on such a connection, only the
  certificate exchange.
- **A pinned parent name.** `parent endpoint` is required: a parent whose
  certificate carries any other CN is refused, even one the same CA signed.
- **The core permission policy.** Every check runs as the principal
  `IcingaAgent:<parent endpoint>`, so `/settings/permissions/policies` decides
  which checks the parent may run. The policy is off by default, as for every
  other transport; with it off, a verified parent can run every query.
- **`allow arguments`** (default `true`, since Icinga passes thresholds as
  arguments). Set it to `false` to allow only argument-less queries and the
  aliases defined on the agent.

**What to do:** nothing unless you load `IcingaAgent`. If you do, enable the
permission policy and grant the parent only the checks it monitors, for
example:

```ini
[/settings/permissions]
enabled = true

[/settings/permissions/policies]
IcingaAgent:icinga-master = CheckSystem.*, CheckDisk.check_drivesize
```

See [Permissions](../concepts/permissions.md).
