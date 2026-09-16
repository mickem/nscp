---
icon: "📥"
modules: [GearmanClient]
action: none
---
**New module: `GearmanClient`, a Mod-Gearman worker and result channel.**
Nothing to do on an existing install — the module is optional and disabled by
default. Enabling it lets a Naemon or Nagios Core keep scheduling its checks
while this agent pulls them off a `gearmand` job server and answers them as
native queries, with no inbound port on the monitored host. It speaks both
flavours of the protocol: ConSol's Mod-Gearman (Naemon) and Nagios
Enterprises' Nagios-Mod-Gearman (Nagios Core 4.5+).

Two ways to deploy it, both configured under `/settings/gearman/worker`:

- `mode = agent` (the default) answers only for the host it runs on; a job for
  any other `host_name` is answered UNKNOWN rather than executed.
- `mode = proxy` answers every check on the queues it registered, whichever
  host the core meant it for — one domain-joined Windows box running the
  checks of a whole hostgroup through `check_nrpe`, `check_wmi` and the rest.
  A proxy reaches everything its own credentials reach, so keep the key to
  those queues to the hosts that should have it.

The same module submits **passive** results into the core's result queue
(`/settings/gearman/client`, channel `GEARMAN`, command `submit_gearman`),
which is what lets a Mod-Gearman installation drop NSCA. The two halves are
independent: configure either one and leave the other empty.

Two things a Nagios operator usually has to unlearn when writing the
`check_command` on the core, since neither fails in a way that names the
cause:

- write `host=$HOSTADDRESS$`, not `-H $HOSTADDRESS$` — a two-character first
  token puts the agent's argument parser into key-value mode and the check
  answers with a help screen;
- write `warning=load gt 80`, not `warning=load>80` — `>` is a metacharacter
  and `allow nasty characters` is `false` by default, as in NRPEServer.

On Windows the module is part of the *Various client plugins* feature of the
installer. See [Mod-Gearman](../scenarios/mod-gearman.md) for the full setup
on either core, and
[Securing NSClient++](securing.md#mod-gearman) for the shared-key model.
