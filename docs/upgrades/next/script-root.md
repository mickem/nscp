---
icon: "🔧 🔒"
modules: [CheckExternalScripts, PythonScript, LUAScript, CheckMKClient, CheckMKServer]
action: conditional
---
**`script root`: run scripts from the fleet tree, or any copy of the scripts
folder.** Every script-carrying module (`CheckExternalScripts`, `PythonScript`,
`LUAScript`, `CheckMKClient`, `CheckMKServer`) now reads a `script root`
setting, from its own section or from `[/settings/default]`. It names the
scripts folder: `${scripts}` by default, `fleet` for `${fleet-folder}/scripts`
(where the fleet sync stages a bundle's `scripts/` entries), `shared` for
`${shared-path}/scripts`, or a folder of your own. Python and Lua read its
`python` and `lua` sub-folders, and an external script is started in the folder
*above* it with a relative command such as `scripts\check.bat` resolved against
that folder - the rule the default layout has always followed, so a scripts
folder moved elsewhere runs unchanged, scripts that call each other by relative
path included. A fleet bundle that ships scripts sets `[/settings/default]
script root = fleet` in its own configuration and needs no host path.
`CheckExternalScripts` already had the key with this meaning and default; it is
now shared by every script module and sets where the scripts run as well. The
one change that reaches hosts which never set the key is the first item below:
on Linux and macOS the agent now sets an external script's working directory.

- **On Linux and macOS an external script now runs in the folder above the
  scripts folder**, set by the agent. The shipped systemd unit already sets
  `WorkingDirectory` to that folder (`${shared-path}`), so a service sees no
  change; an agent started by hand, from an init script or as a container
  entrypoint used to inherit whatever directory it was started in and now
  gets the same one as the service. A script that relied on that inherited
  directory should use an absolute path, or the `scripts/` spelling. If the
  folder does not exist - `script root = fleet` written before the first sync,
  say - scripts run as they did without the key, in the service's own
  directory and rooted at the installation directory, and the log says so
  once.
- **On Windows nothing moves for the built-in layout**: scripts start in the
  installation directory as they always did, including when a `[paths]`
  override has moved `${scripts}` elsewhere. A configured `script root` starts
  them in the folder above it.
- A script imported with `ext-scr add --import` is recorded as
  `scripts/<name>`, relative to the folder it runs in, on every platform; it
  used to be an absolute path on Linux and macOS.
- Whoever can write to the scripts folder can change what the service runs;
  see the
  [security notice](../security/notices.md#script-root-a-service-writable-folder-can-hold-the-scripts-the-service-runs).
