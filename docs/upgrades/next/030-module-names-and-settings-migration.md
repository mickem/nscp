---
icon: "🔒"
modules: [core]
action: conditional
---
**A module is named by a file name, and settings are never migrated *to* a
remote store.** A `[/modules]` entry (or a `Control.LOAD` registry request) whose
right-hand side is a path rather than a single file name is refused with an error
in the log: an absolute value used to replace the module path outright and `..`
walked out of it, which meant naming a module was also naming any file on the
host. The `./modules` fallback now resolves against `${exe-path}` instead of the
process's current directory.

Migrating *to* an `http://` or `https://` context is refused — `nscp settings
--migrate-to` and a `Control.SAVE` naming one. That direction sends this host's
configuration, with the NRPE and NSCA keys, the WEB password and every module's
credentials in it, to whatever the context names; the HTTP backend never
supported saving anyway, so this only replaces "Cannot save settings over HTTP"
with a refusal that says why.

**Importing is unchanged.** `nscp settings --migrate-from <url>`, the MSI's
configuration import and a `Control.LOAD` naming a remote store all still read a
configuration in. Whether that source may be plain `http://` is unchanged too,
and is still decided by `[tls] allow plaintext` in `boot.ini`
([notice 280](../security/notices.md#remote-settings-sources-must-be-https)).

Nothing to do unless a `[/modules]` entry points outside the module path, in
which case move the module there and name it. See the
[security notice](../security/notices.md#core-module-names-as-paths-remote-settings-migration-sensitive-key-names-service-hardening).
