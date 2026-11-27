---
title: "Client targets inherit their parent's security settings"
fixed_in: next
severity: "Low (configuration-dependent)"
modules: [NRPEClient, NSCAClient, NSCANgClient, NSCPClient, NRDPClient, IcingaClient, CheckMKClient, GearmanClient, SMTPClient, SyslogClient, GraphiteClient, CollectdClient]
action: conditional
---
A client target that leaves a key out now takes it from its parent target
(`default` unless it names another), where before most keys were reset to the
built-in default. That includes the keys that decide what a request may do
with the target's configured credentials and how the remote end is
authenticated:

- `allow host override` - set to `true` on `default`, it now also lets a
  request send every other target's configured password or token to a host,
  proxy or TLS setting the request names. Before, every target but `default`
  was held at `false` whatever `default` said.
- `verify mode`, `ca` and, for Icinga, `tls version` - a weaker value on
  `default` (`verify mode = none`) now reaches targets that do not set their
  own; so does a stricter one, which before was dropped.
- Credentials (`password`, NRDP `key`/`password`, Icinga `username`/`password`,
  Gearman `key`) set on `default` are now used by the targets that do not set
  their own.

This brings the agent in line with what the settings describe and what the web
UI's targets editor shows, but a configuration that relied on the reset may
now hand those settings to more targets than intended.

**What to do:** if `default` (or a template target) sets
`allow host override = true`, a relaxed `verify mode`, or credentials meant for
one server only, set the intended value explicitly on each target that should
not inherit it - or move those settings off `default` into the target that
needs them.
