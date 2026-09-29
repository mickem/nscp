---
icon: "🔒"
modules: [core, NRDPClient]
action: none
---
**A setting whose name reads as a credential is redacted whether or not its
module is loaded.** Redaction used to cover only what the modules currently
running had registered, so a password left in `nsclient.ini` for a disabled or
uninstalled module — an NRPE client target password, a WEB password with
`WEBServer` off, an NRDP token — came back in clear from
`GET /api/v2/settings` and from `nscp settings --list` without `--load-all`. The
core now also matches on the key name (`password`, `passwd`, `passphrase`,
`token`, `secret`, `api key`, `credential`, and a bare `key` on a target). The
NRDP `key` alias, the one spelling of that token which was not registered as a
password, is now registered as one. Nothing to do; expect `***` where a settings
dump used to print a value.

The name is only used for *masking*. Moving a value into the Windows Credential
Manager (`use credential manager = true`) still follows what the owning module
declared, so a key that merely reads like a credential is never rewritten in
`nsclient.ini` on the strength of its name — the core's own
`use credential manager` boolean is one of those, and it is excluded from the
match outright. See the [security notice](../security/notices.md#core-module-names-as-paths-remote-settings-migration-sensitive-key-names-service-hardening).
