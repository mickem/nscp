---
icon: "🔒 💥"
modules: [core]
action: conditional
---
**A settings source, include or attachment that is not `https://` is now
refused.** Nothing to do if your agents read a local configuration or use an
`https://` settings server. The remote store is the agent's whole
configuration — `[/modules]`, the external script definitions, the submit
clients' credentials — re-read at boot and on every housekeeping pass, and over
plain `http://` nothing authenticates the server, so anyone on the network path
decides what every agent pointed at it runs. All three paths that fetch one (the
`[settings]` url in `boot.ini`, an `[/includes]` entry inside a fetched file, and
an `[/attachments]` source) now refuse a url that is not `https://` — a url with
no scheme included — and log why. An agent that already has a cached copy keeps
running on it rather than booting empty. Where plain http is what you want, opt
in per host in `boot.ini`:

```ini
[tls]
allow plaintext = true
```

Every plaintext fetch is then logged as `INSECURE`.

On Windows, `boot.ini`'s location is now the `${boot-conf}` path token rather
than a literal baked into the build, so `--path-override boot-conf=...`
relocates it the way the command line already documented and the way it has
always worked on Linux. The default is unchanged: next to the executable.

See
[http settings](../concepts/settings.md#https-settings) and the
[security notice](../security/notices.md#settings-sources-includes-and-attachments-are-refused-over-plain-http).
