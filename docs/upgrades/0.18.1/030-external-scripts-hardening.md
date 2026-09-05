---
icon: "🔒"
modules: [CheckExternalScripts]
action: conditional
---
**CheckExternalScripts hardening.** A hardening pass over the external
scripts module tightened several rough edges: the `ext-scr install` argument
lockdown now writes to the setting the module actually reads (previously it
was a no-op, so a lockdown could silently not apply), the command timeout is
enforced on every execution path with output capped, the `show`/`delete`
sandbox resolves symlinks, and `%`/`^` are blocked on the shell-fallback path.
The default install is unaffected (arguments are off by default). If you rely
on `ext-scr install` to disable arguments, re-run it after upgrading so the
effective setting is written. See the
[security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
