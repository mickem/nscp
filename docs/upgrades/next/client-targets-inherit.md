---
icon: "🔧 🔒"
modules: [NRPEClient, NSCAClient, NSCANgClient, NSCPClient, NRDPClient, IcingaClient, CheckMKClient, GearmanClient, SMTPClient, SyslogClient, GraphiteClient, CollectdClient]
action: conditional
---
**Client targets now inherit every setting from their parent.** A target
(`/settings/<client>/client/targets/<name>`) that leaves a key out now takes
it from the target it names as `parent` - `default` when it names none - and
only `default` itself (or a target with an empty `parent`) falls back to the
built-in default. Before, that only held for keys the agent registers without a
default: `timeout`, `retries`, `allow host override`, the TLS `verify mode` /
`ca` of NSCPClient and CheckMKClient, Icinga's `tls version` / `verify mode` /
`ca`, NRDP's TLS keys, NSCA's `encryption` / `password` / `payload length`,
NSCA-NG's `password` / `use psk` / `insecure`, and so on were silently reset
to the built-in default on every target but `default`, and a section-style
target lost an `address` set on its parent. A one-line target
(`name = host:port`) still uses the address it names, and a target with an
`address` of its own no longer picks up its parent's `host` or `port`.

Two related fixes land with it: the `retries` setting now reaches NRPE, NSCP,
NRDP and Icinga (they read only `retry`, which the setting never set), and
`submit_icinga` no longer overwrites the target's `tls version` with `1.3`
when `--tls-version` is not given.

**Check this if** you set any of these keys on `default` (or on a template
target) and rely on other targets *not* taking them - most importantly
`allow host override = true`, `verify mode = none` or a non-default `ca` on
`default`, which now also apply to every target that does not set its own.
Set the key explicitly on each target that must keep the built-in value. A
configuration written with `--add-defaults` already pins every key on every
target and is unaffected. See the
[security notice](../security/notices.md#client-targets-inherit-their-parents-security-settings).
