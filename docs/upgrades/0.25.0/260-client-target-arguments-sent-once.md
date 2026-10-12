---
icon: "📤"
modules: [CheckMKClient, CollectdClient, GearmanClient, GraphiteClient, IcingaClient, NRDPClient, NRPEClient, NSCAClient, NSCANgClient, NSCPClient, SMTPClient, SyslogClient]
action: none
---
**A request that selects a configured target sends its payload once.** Nothing
to do. Since 0.20.0, a client command run as a query (over REST, NRPE or from a
script) with `target=<name>` naming a configured target sent everything the
request built twice: `nrpe_query target=x command=check_foo argument=a` reached
the remote host as `check_foo!a!a`, `submit_*` sent its message as two lines, and
every `batch=` record was submitted twice. The `--target` option on the command
line had the same problem for exec commands. Each now goes out once. A remote
check that took the repeated arguments in its stride needs nothing; one that
failed on them, or a passive check that showed every batched result twice, now
behaves as configured.
