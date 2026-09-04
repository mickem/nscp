---
icon: "🔒"
modules: [packaging, core]
action: conditional
---
**Modern layout (opt-in, experimental): the shared folder must be a real
directory.** A `%ProgramData%\NSClient++` that is a junction or symbolic link
is refused by the installer, by `nscp settings --migrate-layout modern`, and
at service start (which fails rather than loading a configuration from
behind a link). Relocate the folder with a `[paths]` override in `boot.ini`
instead. Legacy (default) installs are unaffected. See the
[security notice](../security/notices.md#client-credentials-stay-with-their-target-private-script-upload-staging-and-a-junction-proof-shared-folder).
