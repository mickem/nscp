!!! warning "Experimental"

    This module is experimental: its settings, the `$nscp` API and the way a script reports its result may
    still change.

The module runs PowerShell scripts inside the NSClient++ process, the way `LUAScript` and `PythonScript` run
Lua and Python. Each configured script is loaded once into a runspace of its own, where it registers the
checks, command-line commands and channels it answers; the module routes those back into it. A script keeps
its runspace for the life of the module, so whatever it set up while loading - modules, sessions, connections -
is still there when a check arrives.

A runspace runs one pipeline at a time, so the checks one script answers run one after the other, and a
handler cannot ask the agent for a check its own script answers (that is reported as an error rather than
waited on). Checks in different scripts are independent.

See [PowerShell scripts](../../extending/powershell.md) for the script API and worked examples.

#### Requirements

- A **.NET runtime, 8.0 or newer**, built for the same CPU architecture as NSClient++. It is located through
  `hostfxr` exactly as [DotnetPlugins](DotnetPlugins.md) locates it, and the two modules share the runtime:
  whichever starts first decides its version.
- **PowerShell 7 or newer**, installed on the machine. The engine is not shipped with NSClient++; the module
  loads it out of the installation. Windows PowerShell 5.1 cannot be used - it is built on the .NET Framework
  and cannot be loaded into a .NET 8 process.
- The managed script host `modules/dotnet/NSCP.PowerShell.dll` next to `NSCP.Core.dll`. Both are built only
  when the build had the dotnet SDK; the Windows installer carries them under the "PowerShell script support"
  feature.

A module with no scripts configured registers its settings and stops there: no runtime is started and nothing
is reported missing, so enabling it costs nothing until a script is configured.

#### Finding the PowerShell installation

The first of these that holds `System.Management.Automation.dll` is used:

1. the `powershell path` setting
2. the `NSCP_POWERSHELL_HOME` environment variable
3. the `PSHOME` environment variable
4. the folder of the `pwsh` on `PATH`, with symlinks followed
5. the platform's install folders (`%ProgramFiles%\PowerShell\<version>`,
   `/opt/microsoft/powershell/<version>`, `/usr/lib/powershell`, the snap path,
   `/usr/local/microsoft/powershell/<version>`), newest first, a release ahead of a prerelease.

#### Finding a script

A configured script is looked for as written (an absolute path is used as it stands), then under
`script path`, then under a `powershell` folder below it. The `.ps1` extension may be left off, and an entry
with no value names a script after its own alias.

#### Command line

```
nscp powershell list                          # the loaded scripts and what each one answers
nscp powershell execute --script hello.ps1    # run a script's main function
nscp powershell help
```

Anything a script registered with `$nscp.Registry.SimpleCmdline` is reachable the same way.
