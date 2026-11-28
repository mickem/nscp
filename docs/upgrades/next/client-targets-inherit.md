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
target lost an `address` set on its parent.

The address keys follow the same rule, with two exceptions so that a target's
own address is never rewritten by its parent's parts:

| The target sets | It takes from its parent |
|---|---|
| nothing (a section target) | `address`, `host` and `port` |
| `host` and/or `port` only | `address`, and whichever of `host`/`port` it leaves out |
| its own `address` | neither `host` nor `port` (as before) |
| a one-line value (`name = host:port`) | none of the three: it is at the address it names |

**The `retries` setting now reaches NRPE and NRDP, and their default changes
with it.** Those clients read only the `retry` field, which the setting never
set, so they ran with that field's own default of 2 whatever was configured.
They now use the setting, whose documented default is 3 - the value NSCA,
NSCA-NG and collectd already used. An unreachable server is now tried 4 times
by NRPE (one attempt and 3 retries, was 3) and 3 times by NRDP (which counts
`retries` as attempts, was 2). Set `retries = 2` on `default` to keep the old
count. NSCP and Icinga accept the value too but do not retry, and SyslogClient
only logs it.

Also: `submit_icinga` no longer overwrites the target's `tls version` with
`1.3` when `--tls-version` is not given.

**Check this if** you set any of these keys on `default` (or on a template
target) and rely on other targets *not* taking them - most importantly
`allow host override = true`, `verify mode = none` or a non-default `ca` on
`default`, which now also apply to every target that does not set its own.
Set the key explicitly on each target that must keep the built-in value. A
configuration written with `--add-defaults` already pins every key on every
target and is unaffected. See the
[security notice](../security/notices.md#client-targets-inherit-their-parents-security-settings).
