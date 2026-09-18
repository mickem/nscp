---
title: "Settings sources, includes and attachments are refused over plain http"
fixed_in: next
severity: "High for agents configured from an http:// settings server, none otherwise"
modules: [core]
action: conditional
---
A remote settings store is not one setting among many: it is the agent's
entire configuration. It carries `[/modules]`, it carries
`[/settings/external scripts]` — which is arbitrary command execution as
SYSTEM or root by design — and it carries the credentials the submit clients
use. It is read at boot and re-read on every housekeeping pass.

Fetched over `https://` with the default `verify mode = peer`, the agent knows
who served it. Fetched over plain `http://`, nothing authenticates the server
at all: anyone on the network path, and anyone who can answer for the host name
through DHCP or DNS, decides what every agent pointed at that url runs. One
answered query is enough, and nothing appeared in the log.

The `https`-with-verification-disabled case has warned loudly since 0.14
(see [Using TLS](../concepts/settings.md#using-tls)), but
plain http went through the same fetch silently, on all three paths that reach
it: the `[settings]` url in `boot.ini`, an `[/includes]` entry inside a fetched
configuration, and an `[/attachments]` source.

All three now refuse a url that is not `https://` and log why. A url with no
scheme at all counts as plaintext too — it is opened on a plain socket just the
same, and an attachment source is not validated anywhere before it is fetched.
An agent that already has a cached copy of its configuration keeps running on
it rather than booting empty, so a refusal is loud but not an outage.

Where plain http is genuinely what you want — a lab, an isolated network —
`boot.ini` opts in:

```ini
[tls]
allow plaintext = true
```

Every plaintext fetch is then logged as `INSECURE`, on every pass.

**What to do:** if any agent reads its configuration, an include or an
attachment over `http://`, move the settings server to https (the default
`verify mode = peer` verifies against the platform CA bundle) or set
`allow plaintext = true` in that agent's `boot.ini`. Agents with a local
configuration, or an `https://` settings server, are unaffected.
