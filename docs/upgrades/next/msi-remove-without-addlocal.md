---
icon: "📦"
modules: [packaging]
action: conditional
---
**`REMOVE=` on the MSI command line no longer skips the upgrade.** Windows
Installer applies the default feature selection only when the command line
names no features, so an unattended install or upgrade that passed
`REMOVE=<features>` **without** `ADDLOCAL` installed nothing at all: the new
version registered itself in Add/Remove Programs with no files, and Windows
Installer skipped `RemoveExistingProducts`, leaving the previous version
installed and running beside it ([#1256](https://github.com/mickem/nscp/issues/1256)).
The installer now supplies `ADDLOCAL=ALL` itself in that case, so
`msiexec /i NSCP-<version>-x64.msi /quiet REMOVE=NSCPlugins,WEBPlugins`
upgrades the way it reads. Nothing changes for a command line that already
passes `ADDLOCAL`/`ADDDEFAULT`/`ADDSOURCE` — the documented
`ADDLOCAL=ALL REMOVE=<features>` form is still the one to write — nor for
removing a feature from an installation of the same version, where `REMOVE=`
alone still means only "take this feature away". If a host ended up with two
NSClient++ entries in Add/Remove Programs from an earlier attempt, uninstall
the older one before upgrading.
