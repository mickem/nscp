# PowerShellScript

Loads and processes PowerShell scripts in-process, so a script can register checks and talk back to NSClient++

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


## Enable module

To enable this module and allow using the commands you need to add `PowerShellScript = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
PowerShellScript = enabled
```

## Samples

_Feel free to add more samples [on this page](https://github.com/mickem/nscp/blob/master/docs/samples/PowerShellScript_samples.md)_

A script registers what it answers while it loads, then answers it. `hello.ps1` in the script folder
(`${scripts}` by default):

```powershell
function Check-Hello {
    param([string]$command, [string[]]$arguments)

    $name = 'world'
    foreach ($argument in $arguments) {
        if ($argument -like 'name=*') { $name = $argument.Substring(5) }
    }
    return @('ok', "Hello $name", "'greetings'=1")
}

$nscp.Registry.SimpleQuery('check_hello', 'Say hello', 'Check-Hello')
```

```ini
[/modules]
PowerShellScript = enabled

[/settings/powershell/scripts]
hello = hello.ps1
```

```
nscp client --module PowerShellScript --boot --query check_hello name=there
Hello there|'greetings'=1

nscp client --module PowerShellScript --boot --exec list
Loaded PowerShell scripts:
  hello: /usr/lib/nsclient/scripts/hello.ps1
    query check_hello - Say hello
```

A script can call back into the agent while it answers: run another check, read a setting, submit a passive
result or write to the log.

```powershell
function Check-Callback {
    param([string]$command, [string[]]$arguments)

    $inner = $nscp.Core.SimpleQuery('check_cpu', 'warning=80')
    $port = $nscp.Settings.GetInt('/settings/WEB/server', 'port', 8443)
    $nscp.Info("cpu came back $($inner.Status)")
    return @{ code = $inner.Status; message = "cpu: $($inner.Message) (web on $port)" }
}

$nscp.Registry.SimpleQuery('check_callback', 'Check the CPU and say where the web server is', 'Check-Callback')
```

See [PowerShell scripts](../../extending/powershell.md) for the whole `$nscp` API.


## Configuration

| Path / Section                                  | Description        |
|-------------------------------------------------|--------------------|
| [/settings/powershell](#powershell)             | POWERSHELL         |
| [/settings/powershell/scripts](#powershell-scripts) | PowerShell scripts |


### POWERSHELL <a id="/settings/powershell"></a>

Section for the PowerShellScript module: runs PowerShell scripts inside NSClient++.

| Key                                          | Default Value         | Description              |
|----------------------------------------------|-----------------------|--------------------------|
| [execution policy](#execution-policy)        |                       | Execution policy         |
| [plugin path](#plugin-path)                  | ${module-path}/dotnet | Plugin path              |
| [powershell path](#powershell-installation)  |                       | PowerShell installation  |
| [runtime path](#net-runtime-root)            |                       | .NET runtime root        |
| [script path](#script-path)                  | ${scripts}            | Script path              |


```ini
# Section for the PowerShellScript module: runs PowerShell scripts inside NSClient++.
[/settings/powershell]
script path=${scripts}
```

#### Script path <a id="/settings/powershell/script path"></a>

Folder the configured scripts are looked for in (a 'powershell' folder below it is searched as well).


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/powershell](#/settings/powershell) |
| Key:           | script path                                   |
| Default value: | `${scripts}`                                  |


#### Execution policy <a id="/settings/powershell/execution policy"></a>

PowerShell execution policy to run the scripts under (Restricted, AllSigned, RemoteSigned, Unrestricted, Bypass). Leave empty to use the machine's policy, which is what pwsh itself would apply. Windows only: there is no execution policy on other platforms.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/powershell](#/settings/powershell) |
| Key:           | execution policy                              |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | _N/A_                                         |


#### PowerShell installation <a id="/settings/powershell/powershell path"></a>

Folder of the PowerShell 7 installation to load the engine from (the folder holding pwsh and System.Management.Automation.dll). Leave empty to use the PSHOME environment variable, the pwsh on PATH or the platform's default install folders. Windows PowerShell 5.1 cannot be used: it is built on the .NET Framework.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/powershell](#/settings/powershell) |
| Key:           | powershell path                               |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | _N/A_                                         |


#### Plugin path <a id="/settings/powershell/plugin path"></a>

Folder holding NSCP.Core.dll and NSCP.PowerShell.dll (the managed script host shipped with NSClient++). Falls back to ${exe-path}/modules/dotnet when the folder does not exist.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/powershell](#/settings/powershell) |
| Key:           | plugin path                                   |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | `${module-path}/dotnet`                       |


#### .NET runtime root <a id="/settings/powershell/runtime path"></a>

Root folder of the .NET installation to host (the folder containing 'host/fxr'). Leave empty to use DOTNET_ROOT, the registered install location or the platform's default install folders.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/powershell](#/settings/powershell) |
| Key:           | runtime path                                  |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | _N/A_                                         |


### PowerShell scripts <a id="/settings/powershell/scripts"></a>

A list of scripts to load: &lt;alias&gt; = &lt;script.ps1&gt;, where the script is looked for under the script path (and in a 'powershell' folder below it) unless it is an absolute path. Every script runs once on load so it can register the checks it answers.


```ini
# A list of scripts to load: <alias> = <script.ps1>, ...
[/settings/powershell/scripts]
hello=hello.ps1
```
