---
title: "collectd metrics no longer fan out over every local interface"
fixed_in: 0.18.2
severity: "Low"
modules: [CollectdClient]
action: conditional
---
A `CollectdClient` target with no address configured falls back to collectd's
default multicast group, `239.192.74.66:25826`. For a multicast target the
sender used to enumerate every local interface of the matching address family
and send a copy of every datagram through each one, so a half-configured target
put the machine's host name, CPU, memory, uptime and process counts on every
attached layer-2 segment — including ones the operator never meant to reach,
such as a DMZ or guest-network NIC. The collectd network protocol carries no
credentials, and these datagrams were unauthenticated cleartext.

The fan-out is now opt-in. A multicast target sends one copy through the
interface the routing table picks (`multicast interface = auto`, the default).
`multicast interface = all` restores the old behaviour, and a comma-separated
list of local IP addresses restricts the fan-out to exactly those interfaces —
an entry that is not a usable local address for the target's family is reported
and skipped, and a target whose whole list is unusable sends nothing rather
than falling back to the default route.

The metrics themselves are unchanged: the collectd binary protocol as this
module speaks it is unsigned cleartext, so a collectd target still belongs on a
trusted network or inside a tunnel.

**What to do:** nothing on a unicast target — the setting is ignored there. If
you rely on a multicast target reaching several segments, set
`multicast interface = all` (or list the local addresses to send through) on
that target.
