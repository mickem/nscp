---
icon: "🔌"
modules: [PowerShellScript, DotnetPlugins, packaging]
action: none
---
**PowerShell scripts can run inside the agent again, on Windows and Linux.** The
new `PowerShellScript` module runs `.ps1` files in-process the way `LUAScript`
and `PythonScript` run Lua and Python: a script is loaded once, registers the
checks, command-line commands and channels it answers, and talks back to the
agent through a `$nscp` object (queries, passive results, settings, logging).
Nothing to do on a default install — the module is not loaded unless you add
`PowerShellScript = enabled` to `[/modules]`, and the `ps1` wrapping in
`CheckExternalScripts` is unchanged. It needs a .NET runtime (8.0 or newer) and
**PowerShell 7** on the machine; neither is bundled, and Windows PowerShell 5.1
cannot be used because it is built on the .NET Framework. Scripts are
configured under `[/settings/powershell/scripts]` (`<alias> = <script.ps1>`),
see the
[PowerShellScript reference](../reference/generic/PowerShellScript.md) and
[PowerShell scripts](../extending/powershell.md). The module is **experimental**:
its settings, the `$nscp` API and the way a script reports its result may still
change.

The old `CheckPowershell` module is gone. It had not been built on any platform
for years — its `module.cmake` disabled it, and it referenced a
`System.Management.Automation.dll` by absolute path on the author's build
machine — so no release ever shipped it and no configuration can refer to it.
Its `[/modules/powershell/commands]` section, where each key was a line of
PowerShell to run, has no equivalent: a script does the same by registering a
check with `$nscp.Registry.SimpleQuery`, and a handler that reports with
`Write-Host` is read the same way the old module read it.

.NET plugins gain one method and get one fix in the managed API
(`NSCP.Core.dll`). `ICore.expandPath(path)` expands `${base-path}`,
`${scripts}` and the other path variables, which a settings value read through
`ICore.settings` does not come back expanded. A plugin that does not implement
`ICore` itself (the usual case — plugins consume it) needs no change; a test
double that does implement it has to add the method.
`SettingsHelper.getKeys(path)` now returns the **keys** configured under
`path`, which is what its name says and what the native `get_keys` has always
returned; it previously returned the paths of the sections *below* `path` and
never a key at all. The old answer is available as the new
`SettingsHelper.getSections(path)`, so a plugin that relied on it changes one
call.
