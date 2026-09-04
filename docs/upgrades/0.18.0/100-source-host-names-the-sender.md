---
icon: "📡"
modules: [NRPEClient, NSCAClient, NSCANgClient, NRDPClient, SMTPClient, GraphiteClient, SyslogClient, IcingaClient, CollectdClient, Op5Client, NSCPClient, ElasticClient, CheckMKClient]
---
**`--source-host` / `--sender-host` now name the sending host, on every
client module.** They were registered against the *destination* container,
where the well-known `host` key is routed into the typed address field — so
naming a source host silently redirected the connection to it, and the
sender the handler reads was never set. `SMTPClient` and `NRDPClient` had
each worked around this by registering their own copies, which made the
option name ambiguous and so **unusable on those two modules** (`option
'--source-host' is ambiguous`). The options are registered once now, against
the sender. If you had scripted around the old behaviour by passing
`--source-host` to redirect a connection, use `--host` or `--address` for
that instead.
