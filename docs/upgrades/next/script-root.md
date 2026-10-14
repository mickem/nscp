---
icon: "🔧 🔒"
modules: [CheckExternalScripts, PythonScript, LUAScript, CheckMKClient, CheckMKServer]
action: conditional
---
**`script root`: run scripts from the fleet tree, or any folder laid out like the
install.** Every script-carrying module (`CheckExternalScripts`, `PythonScript`,
`LUAScript`, `CheckMKClient`, `CheckMKServer`) now reads a `script root`
setting, from its own section or from `[/settings/default]`. The value is
`fleet` (`${fleet-folder}`, where the fleet sync stages a bundle's `scripts/`
entries), `shared` (`${shared-path}`), a folder, or empty for the built-in
layout. A configured root has the install layout below it - `scripts/`,
`scripts/python`, `scripts/lua` - and an external script is started with the
root as its working directory, with a relative command such as
`scripts\check.bat` resolved against it, so scripts that call each other by
relative path keep working wherever the tree is placed. A fleet bundle that
ships scripts sets `[/settings/default] script root = fleet` in its own
configuration and needs no host path. Nothing changes on an install that does
not set it.

- **`CheckExternalScripts` already had a `script root` key, and its meaning
  changed.** It used to name the scripts folder itself (default `${scripts}`)
  and only bounded `ext-scr show`, `delete` and `add --import`. It now names
  the folder *above* `scripts/`, bounds the same operations to
  `<root>/scripts`, and also sets where every external script runs. If you had
  set it to a folder of your own, move the scripts into a `scripts`
  sub-folder of it, or point it at the parent. An unset key behaves as
  before.
- **On Linux and macOS an external script now runs in the root** when one is
  configured; with none configured it inherits the service's working
  directory as it always has. On Windows the working directory was always the
  installation directory, and still is for the built-in layout.
- A script imported with `ext-scr add --import` under a configured root is
  recorded as `scripts/<name>`, relative to the root, rather than as an
  absolute path.
- Whoever can write to the root can change what the service runs; see the
  [security notice](../security/notices.md#script-root-a-service-writable-folder-can-hold-the-scripts-the-service-runs).
