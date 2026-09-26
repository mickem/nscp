---
icon: "🔒"
modules: [core]
action: conditional
---
**A module is named by a file name, and settings migration stays local.** A
`[/modules]` entry (or a `Control.LOAD` registry request) whose right-hand side
is a path rather than a single file name is refused with an error in the log: an
absolute value used to replace the module path outright and `..` walked out of
it, which meant naming a module was also naming any file on the host. The
`./modules` fallback now resolves against `${exe-path}` instead of the process's
current directory. Migrating settings against an `http://` or `https://` context
is refused — `nscp settings --migrate-from` / `--migrate-to` as well as the
`Control.LOAD` / `Control.SAVE` registry requests, since the refusal now sits on
the core rather than on one caller. A remote settings source belongs in
`boot.ini`. Nothing to do unless a `[/modules]` entry points outside the module
path, in which case move the module there and name it. See the
[security notice](../security/notices.md#core-module-names-as-paths-remote-settings-migration-sensitive-key-names-service-hardening).
