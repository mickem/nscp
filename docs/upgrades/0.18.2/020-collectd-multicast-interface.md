---
icon: "🔒 💥"
modules: [CollectdClient]
action: conditional
---
**A multicast collectd target now sends through one interface, not all of
them.** A `CollectdClient` target with no address goes to collectd's default
multicast group (`239.192.74.66:25826`), and every such target used to send a
copy of the metrics through every local interface of the matching address
family — putting them on segments the operator never meant to reach. The new
per-target `multicast interface` setting decides this: `auto` (the default)
sends one copy through the interface the routing table picks, `all` restores
the previous fan-out, and a comma-separated list of local IP addresses sends
through exactly those. Unicast targets are unaffected. If you depend on a
multicast target reaching several segments, set `multicast interface = all` on
it. See the
[security notice](../security/notices.md#collectd-metrics-no-longer-fan-out-over-every-local-interface).
