---
icon: "🔧"
modules: [NRPEServer, NSCAServer, NSClientServer, CheckMKServer, WEBServer]
action: conditional
---
**A rejected connection now logs one line that says why and where to fix it.**
When an address is not in `allowed hosts`, the server logs which address was
rejected, what the list holds and which settings section sets it:

```
Rejected connection from: 192.168.5.7, not in allowed hosts (127.0.0.1); set `allowed hosts` in /settings/NRPE/server, or in /settings/default if that section does not set it
```

Nothing to do unless you match these lines in a log monitor or a
fail2ban-style filter. The `Rejected connection from: <address>` prefix is
unchanged, but the separate `IP address not allowed` and
`allowed_hosts is empty - rejecting all connections ...` lines are gone (their
content is now part of the one line), and an IPv4 client on a dual-stack
listener is logged by its IPv4 address (`10.1.2.3`) rather than as
`::ffff:10.1.2.3`. See the
[FAQ](../faq.md#41-rejected-connection-from-your-ip-address-here).
