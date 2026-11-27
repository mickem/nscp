---
icon: "🔧"
modules: [ElasticClient, Op5Client, NRPEClient, NSCAClient, NSCANgClient, NRDPClient, NSCPClient, SMTPClient, SyslogClient, GraphiteClient, CollectdClient, CheckMKClient, GearmanClient, IcingaClient]
action: none
---
**Web UI: a targets editor for the client modules.** Nothing to do. The
targets of the twelve client modules (NRPE, NSCA, NSCA-NG, NRDP, NSCP, SMTP,
syslog, Graphite, collectd, check_mk, Gearman and Icinga) are edited on one
card per module: each target with its address, how it is protected and what
weakens it, and a dialog that edits it by section - connection, security,
what is sent - with the rarely changed keys under *Advanced* and *Show config*
for an INI fragment.

Security is one choice where a protocol has modes: TLS with the server
verified, TLS without, legacy anonymous NRPE, or none for NRPE, NSCP and
check_mk; a pre-shared key or the server's certificate for NSCA-NG;
encrypted or not for Gearman. A stored configuration that matches none of them
is shown as *Custom* and left alone. The card says plainly what the agent will
refuse (an NSCA-NG certificate target that does not verify the server, an
unencrypted Gearman target without its acknowledgement, a login over plain
SMTP) and what is weak (an unverified server, plain HTTP, an empty NSCA key).
NSCA targets offer the secure ciphers only and leave TLS to expert mode, as the
NSCA server page does.

The `hostname` of the ten clients that report one (NSCA, NSCA-NG, NRDP,
syslog, Graphite, collectd, Gearman, Icinga, Elastic, op5) is picked rather
than typed: the computer's name as reported, in lowercase or uppercase, or a
name of its own built from placeholders (`${host_lc}`, `${domain}`,
`${address_ipv4}`, ...), including the full-name `${hostname}` forms the
setting's description leaves out.

NRPE and NSCP targets have a *Test* button: NRPE asks the server for its
version, NSCP runs `check_ok` on the remote agent. The passive clients have
none, since the only test is submitting a real result.

The `default` target is the one used when a request names none or an unknown
one. Inheritance between targets (`parent`) is left to expert mode.
