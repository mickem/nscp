---
icon: "🔒"
modules: [CheckExternalScripts]
action: conditional
---
**External scripts with `user`, `domain` or `password` set are refused on
Linux.** The run-as keys of a script section are implemented by the Windows
launcher only; the Linux launcher ignored them and ran the script as the
service account (root on a manual `nscp service` run), so a script sandboxed
with `user = nobody` was not sandboxed at all. Such a command now returns
UNKNOWN with a message pointing at `sudo` and the script does not run. If you
set these keys on Linux, remove them and put the identity change in the
command itself, granting it in `sudoers`:

```ini
[/settings/external scripts/scripts/check_as_nobody]
command = sudo -n -u nobody /usr/lib/nagios/plugins/check_something
```

Windows is unaffected. See the
[security notice](../security/notices.md#external-scripts-run-as-settings-were-silently-ignored-on-linux).
