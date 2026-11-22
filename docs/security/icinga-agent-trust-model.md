---
title: "IcingaAgent: what a connected Icinga parent may run"
fixed_in: 0.27.0
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
- **What an unverified first connection gives away.** No check runs on it,
  but the agent sends its `ticket` in the first message, to whoever answers at
  `parent host`. A ticket does not expire and is not used up: its holder can
  have the real master sign a certificate for the agent's endpoint name and
  then connect to the master as that agent - reporting check results for the
  host and receiving the checks meant for it. It can only be withdrawn by
  changing `TicketSalt` on the master, which invalidates every ticket but
  revokes no certificate already signed with one. The
  same peer can answer the request with a CA of its own; the agent stores it
  and from then on treats that peer as its parent, running the queries the
  permission policy grants (all of them while the policy is off). The agent
  logs an error whenever it sends a ticket this way.
- **What the agent accepts as its certificate.** Only one for its own key,
  with its own endpoint name as CN, issued by the CA delivered with it (or
  the one already stored). Anything else is refused and nothing is replaced.
- **A pinned parent name.** `parent endpoint` is required: a parent whose
  certificate carries any other CN is refused, even one the same CA signed.
- **The core permission policy.** Every check runs as the principal
  `IcingaAgent:<parent endpoint>`, so `/settings/permissions/policies` decides
  which checks the parent may run. The policy is off by default, as for every
  other transport; with it off, a verified parent can run every query.
- **`allow arguments`** (default `true`, since Icinga passes thresholds as
  arguments). Set it to `false` to allow only argument-less queries and the
  aliases defined on the agent.

The `ticket` is a password setting, so the settings API and the web UI mask
it; it is still stored in the configuration file like any other secret
there.

**What to do:** nothing unless you load `IcingaAgent`. If you do, install the
master's `ca.crt` with the agent (or set `parent fingerprint`) so the first
connection is verified, and set `trust on first use = false` on a managed
fleet. Rely on trust on first use only where you trust the network for that
one connection, and if a ticket may have reached the wrong peer, change
`TicketSalt` on the master. Enable the permission policy and grant the parent only the checks it
monitors, for example:

```ini
[/settings/permissions]
enabled = true

[/settings/permissions/policies]
IcingaAgent:icinga-master = CheckSystem.*, CheckDisk.check_drivesize
```

See [Permissions](../concepts/permissions.md).
