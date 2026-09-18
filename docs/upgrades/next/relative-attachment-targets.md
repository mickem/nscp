---
icon: "🔧"
modules: [core]
action: conditional
---
**A relative `[/attachments]` target now resolves against `${shared-path}`.**
Nothing to do unless an attachment key is a relative path
(`scripts/check_lsi_raid.pl = https://…`) and you relied on where it used to
land. Such a key used to be resolved against the process's working directory:
the installation folder when `nscp test` was started from there, and nowhere
useful otherwise - the Windows service starts in `System32`, and the MSI's
`ImportConfig` step wherever `msiexec` runs. In both cases the download had no
folder to be written to, the agent logged
`Failed to find cached settings: scripts/check_lsi_raid.pl.tmp`, and an install
with `CONFIGURATION_TYPE=https://…` or `IMPORT_CONFIG=https://…` discarded the
configuration it had been asked to import
([#1557](https://github.com/mickem/nscp/issues/1557)). A relative key is now
anchored on `${shared-path}` - the installation folder on Windows,
`/usr/lib/nsclient` on Linux - so `scripts/…` lands in the scripts folder
whichever way the agent was started, and the target's folder is created when
it does not exist yet. A key spelled with a token (`${scripts}/…`,
`${shared-path}/…`) or an absolute path is unchanged. The one visible
difference on Linux: a relative key used to be written under the service's
working directory (`/` for a systemd unit), and now goes under the package
directory, which the service account cannot write to - use `${data-path}/…`
or another writable location there.
