#### About `submit_graphite`

`submit_graphite` sends a check result to a Graphite server over the plaintext
line protocol (Carbon). Unlike the Nagios-flavoured submit commands, Graphite
stores **numbers over time**, not states with messages — so what actually gets
sent is the check's performance data.

The usual way to use it is not to call it directly but to route results to it:
give a scheduled check `target=graphite` (or add `GRAPHITE` to the channels a
check reports on) and the module forwards each result as it is produced. Calling
the command by hand is mainly useful for testing that the connection and metric
paths are right.

##### Metric paths

Two settings on the module decide where the values land:
`path` for performance data and `status path` for the status value, both written
as Graphite dotted paths with the usual `${hostname}`, `${check_alias}` and
`${perf_alias}` placeholders. `send perfdata` and `send status` turn each half
on or off — sending status as a number is often not what you want, since a
Graphite dashboard renders `0/1/2/3` poorly compared to a real alerting system.

Get the path template right before pointing a fleet at it: Carbon creates a
whisper file per distinct metric path on first write, so a template that
interpolates something volatile (a PID, a timestamp, an unsanitised check name)
will litter the storage with files that then have to be cleaned up by hand.

##### Transport

Carbon's plaintext protocol is **unauthenticated**, and by default this module
speaks it in the clear. Set `ssl = true` to wrap the connection in TLS, and
supply `ca`, `certificate` and `certificate key` for a Carbon endpoint that
requires them. On an untrusted network, treat the plaintext default as
unsuitable — anyone on the path can both read and inject metrics.
