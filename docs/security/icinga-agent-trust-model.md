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
  the CA the parent issued when it signed the agent's certificate. Icinga has
  no public CA, so the first connection needs the CA installed up front
  (`ca.crt` in the certificate directory) or the parent's certificate pinned
  (`parent fingerprint`, as `icinga2 node setup --trustedcert`). Without
  either it cannot be verified, and by default (`trust on first use = true`)
  the agent makes it anyway but runs no checks on it, only the certificate
  exchange; `trust on first use = false` refuses it.
- **A pinned parent name.** `parent endpoint` is required: a parent whose
  certificate carries any other CN is refused, even one the same CA signed.
- **The core permission policy.** Every check runs as the principal
  `IcingaAgent:<parent endpoint>`, so `/settings/permissions/policies` decides
  which checks the parent may run. The policy is off by default, as for every
  other transport; with it off, a verified parent can run every query.
- **`allow arguments`** (default `true`, since Icinga passes thresholds as
  arguments). Set it to `false` to allow only argument-less queries and the
  aliases defined on the agent.

**What to do:** nothing unless you load `IcingaAgent`. If you do, install the
master's `ca.crt` with the agent (or set `parent fingerprint`) so the first
connection is verified, and set `trust on first use = false` on a managed
fleet. Enable the permission policy and grant the parent only the checks it
monitors, for example:

```ini
[/settings/permissions]
enabled = true

[/settings/permissions/policies]
IcingaAgent:icinga-master = CheckSystem.*, CheckDisk.check_drivesize
```

See [Permissions](../concepts/permissions.md).
