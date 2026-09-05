---
icon: "🔒"
modules: [CheckMKClient]
action: conditional
---
**check_mk client targets: configured TLS settings are honoured again.**
`check_mk_target_object::read()` added the SSL keys (`use ssl`,
`certificate`, `verify mode`, `ca`, …) to its settings registry but never
called `register_all()`/`notify()`, so values set on a
`[/settings/check_mk/client/targets/…]` section were silently ignored and
the client connected in plaintext regardless of configuration. The keys are
read (and documented) again. If you configured `use ssl = true` on a
check_mk target, the connection becomes TLS on upgrade — make sure the
server side actually speaks TLS, or the check starts failing. Details in the
[security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
