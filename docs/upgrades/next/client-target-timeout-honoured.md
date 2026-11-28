---
icon: "⏱️"
modules: [SyslogClient, SMTPClient, CheckMKClient, NSCAClient, NSCANgClient]
action: conditional
---
**A target's `timeout` now takes effect in the Syslog, SMTP, Check_MK, NSCA and NSCA-NG clients.**
These clients read `timeout` from the wrong place and always used 30 seconds,
whatever the target, the `default` target or a `timeout=` option on the
command said. They now use the configured value, as IcingaClient, NRDPClient,
NSCPClient and CollectdClient already did. You only need to act if you set a
`timeout` on one of these targets: it now bounds the connection. Check that it
is long enough for a slow submission server. A target that sets no `timeout`
still gets 30, because every target, the implicit `default` one included,
inherits the documented default.

A `timeout` of `0` or less keeps the 30-second default in SMTPClient,
CheckMKClient, NSCAClient and NSCANgClient. In those clients a 0 would end the
connection before it started, so it cannot mean "no limit" the way it does in
CollectdClient. SyslogClient sends over UDP and never waits on the value, so it
keeps a configured 0. SyslogClient also reads `retries`, falling back to
`retry`, the way CollectdClient does. It used to look `retry` up where it never
landed.
