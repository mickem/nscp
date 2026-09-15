---
title: "External scripts: run-as settings were silently ignored on Linux"
fixed_in: next
severity: "Medium for Linux setups that set `user` on a script, none otherwise"
modules: [CheckExternalScripts]
action: conditional
---
A script section under `[/settings/external scripts/scripts/<alias>]` accepts
`user`, `domain` and `password` to run the command as another account. The
keys are registered on every platform, but only the Windows launcher
(`LogonUser` + `CreateProcessAsUser`) implemented them. The Linux launcher
never read them and logged nothing, so an operator who sandboxed an untrusted
or argument-taking script with `user = nobody` got it executed as the service
identity - `nsclient` on the packaged install, root on a manual
`nscp service` run - with a plaintext password in the ini for nothing.

Linux already has a well-understood mechanism for running a command as
another user, so rather than re-implementing account switching in the agent
the launcher now refuses: a script with any of the three keys set returns
UNKNOWN with a message pointing at `sudo`, the error is logged, and the
script is not started. Run the command through `sudo -u <user>` (with a
matching `sudoers` rule, `NOPASSWD` and `-n` so the check can never block on
a password prompt) and drop the keys. The settings descriptions say the keys
are Windows-only. Windows behaviour is unchanged.

**What to do:** on Linux, grep `nsclient.ini` for `user =`, `domain =` and
`password =` under the external-script sections. For each hit, move the
identity change into `command = sudo -n -u <user> ...`, add the sudoers rule,
and delete the keys; until you do, that check reports UNKNOWN. Nothing to do
on Windows or if you never set them.
