# PowerShell Scripts

!!! warning "Experimental"

    The `PowerShellScript` module is experimental: the settings, the `$nscp` API and the way a script reports
    its result may still change. The module is not installed as part of a default upgrade path yet - see
    [Installing](#installing-and-running) below.

The **PowerShellScript** module runs PowerShell scripts *inside* NSClient++, the way the
[Lua](lua.md) and [Python](python.md) modules run Lua and Python. A script is loaded once, keeps whatever it
set up, registers the checks it answers, and can query other checks, submit passive results, read settings and
log through the agent.

This is a different thing from the `ps1` wrapping in
[external scripts](../reference/check/CheckExternalScripts.md), which starts `powershell.exe` for every
check. That still works and is still the right answer for a plain Nagios-style plugin; this module is for a
script that wants to talk back to the agent, or that is too expensive to start on every check.

## How it fits together

PowerShell is a .NET library, so the engine lives on the managed side of the same boundary the
[.NET plugins](dotnet.md) use:

```
nscp ──loads──> PowerShellScript (native module)
                  │  hostfxr: starts the installed .NET runtime
                  ├──loads──> modules/dotnet/NSCP.Core.dll        (managed plugin API)
                  └──loads──> modules/dotnet/NSCP.PowerShell.dll  (the script host)
                                 │  loads the engine from an installed PowerShell 7
                                 └──runs──> your-script.ps1       (one runspace per script)
```

### Requirements

- A **.NET runtime, 8.0 or newer**, built for the same CPU architecture as NSClient++ (a 32-bit agent needs the
  x86 runtime). It is located exactly as the `DotnetPlugins` module locates it; see the
  [DotnetPlugins reference](../reference/generic/DotnetPlugins.md) for the search order and the `runtime path`
  setting.
- **PowerShell 7 or newer**, installed on the machine. NSClient++ does not ship the engine: it is roughly 150 MB
  of assemblies, several times the size of the whole agent. The module loads it out of the installation, which
  is what a PowerShell binary module does.

    **Windows PowerShell 5.1 cannot be used.** It is built on the .NET Framework and cannot be loaded into a
    .NET 8 process at all. Scripts that only use language features and cmdlets present in both run unchanged
    under 7; anything reaching for a 5.1-only snap-in does not.

    The module looks for the installation in this order, and uses the first one that holds
    `System.Management.Automation.dll`:

    1. the `powershell path` setting
    2. the `NSCP_POWERSHELL_HOME` environment variable
    3. the `PSHOME` environment variable
    4. the folder of the `pwsh` on `PATH` (symlinks are followed, so the `/usr/bin/pwsh` most packages install
       leads to the real installation)
    5. `%ProgramFiles%\PowerShell\<version>` on Windows; `/opt/microsoft/powershell/<version>`,
       `/usr/lib/powershell` and the snap path on Linux; `/usr/local/microsoft/powershell/<version>` on macOS.
       Newest first, and a release ahead of a prerelease of the same number.

## Writing a script

A script registers what it answers while it loads, then answers it. The handler is the name of a function in
the script, or a script block:

```powershell
# hello.ps1

function Check-Hello {
    param([string]$command, [string[]]$arguments)

    $name = 'world'
    foreach ($argument in $arguments) {
        if ($argument -like 'name=*') { $name = $argument.Substring(5) }
    }
    return @('ok', "Hello $name", "'greetings'=1")
}

$nscp.Registry.SimpleQuery('check_hello', 'Say hello', 'Check-Hello')
$nscp.Info("hello.ps1 loaded")
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
```

`$arguments` holds the check's arguments as they were sent. REST and the one-shot client path both pass a
`key=value` pair as one token; the interactive CLI's `-a key=value` splits it into `--key` and `value`, so parse
what you are given rather than counting on a position.

### Lifecycle

| What                    | When                                                                                  |
|-------------------------|---------------------------------------------------------------------------------------|
| Top-level code          | Once, when the module loads the script. This is where you register.                    |
| `function on_start`     | Once, after every configured script has been loaded and has registered.                |
| Registered handlers     | Every time the check, command or channel they were registered for is used.             |
| `function main`         | Only by `nscp powershell execute --script <file>`, with the remaining arguments.        |

Each script gets its **own runspace**, kept for the life of the module, so the functions, variables, modules
and connections it set up while loading are what its handlers run against. A runspace runs one pipeline at a
time, so calls into one script are serialised: two checks answered by the same script run one after the other.
Split them across scripts if you need them to overlap.

That also sets the one limit on `$nscp.Core.SimpleQuery`: a handler may ask for any check *except* one its own
script answers. Doing so fails the check with "A PowerShell script cannot run a command it answers itself"
rather than waiting for a pipeline that cannot start. Put the check it needs in another script - a query that
leaves the script and comes back through the agent works fine.

### Returning a result

A check handler may return any of these:

| Returned                                             | Read as                                             |
|------------------------------------------------------|-----------------------------------------------------|
| `@('ok', 'Everything is fine', "'load'=0.5")`        | status, message, performance data                   |
| `@{ code = 'ok'; message = '...'; perf = '' }`       | the same, by name (`status`/`result`, `msg`, `perfdata` also work) |
| `'OK: everything is fine\|load=0.5'`                 | message only; the status is OK                      |
| anything else                                        | the objects as text, one per line; the status is OK  |

A status is `ok`, `warning` (`warn`), `critical` (`crit`) or `unknown`, or the number `0`-`3`. Anything else
reads as `unknown` rather than silently passing as OK.

A returned message containing a `|` is split into message and performance data, the same way a Nagios plugin's
output is. A handler that writes with `Write-Host` instead of returning is read from the information stream,
which is what a script ported from the old `[/modules/powershell/commands]` module does.

An error - a terminating exception, or anything written to the error stream - makes the check `UNKNOWN` and is
logged with the script's path.

## The `$nscp` object

Every script is handed `$nscp`, its link back into the agent.

| Member                                | What it does                                                        |
|---------------------------------------|---------------------------------------------------------------------|
| `$nscp.Info/Error/Warning/Debug/Critical(message)` | Write one line to the agent's log                      |
| `$nscp.Log(level, message)`           | The same, with the level as a word                                   |
| `$nscp.Alias`                         | The alias the module is loaded under (`powershell` by default)       |
| `$nscp.ScriptAlias`                   | The alias this script is configured under                            |
| `$nscp.ScriptPath`                    | The full path of this script                                         |
| `$nscp.Core`                          | Calls into the agent                                                 |
| `$nscp.Registry`                      | What this script answers                                             |
| `$nscp.Settings`                      | The agent's settings                                                 |

### `$nscp.Registry`

```powershell
$nscp.Registry.SimpleQuery('check_hello', 'Say hello', 'Check-Hello')
$nscp.Registry.SimpleCmdline('hello', { param($command, $arguments) "called with $arguments" })
$nscp.Registry.SimpleSubscription('my_channel', 'Handle-Result')
```

| Method                                          | Handler is called as                                                          |
|-------------------------------------------------|-------------------------------------------------------------------------------|
| `SimpleQuery(command, description, handler)`    | `handler <command> <arguments>` - answers `check_nrpe -c <command>`, REST, ... |
| `SimpleCmdline(command, handler)`               | `handler <command> <arguments>` - answers `nscp powershell <command>`          |
| `SimpleSubscription(channel, handler)`          | `handler <channel> <command> <status> <message> <perf>`                        |

`handler` is the name of a function in the script, or a script block. Arguments are passed positionally, so a
handler declares them with a plain `param(...)`.

### `$nscp.Core`

```powershell
$result = $nscp.Core.SimpleQuery('check_cpu', 'warning=80')
if ($result.Code -ne 0) { $nscp.Warning("cpu is $($result.Status): $($result.Message)") }

$nscp.Core.SimpleSubmit('my_channel', 'check_hello', 'ok', 'Hello', "'greetings'=1")
```

| Method                                                       | What it does                                        |
|--------------------------------------------------------------|-----------------------------------------------------|
| `SimpleQuery(command, arguments...)`                         | Run another check; returns `Code`, `Status`, `Message`, `Perf` |
| `SimpleExec(target, command, arguments...)`                  | Run a command-line style command; returns its output lines |
| `SimpleSubmit(channel, command, status, message, perf)`      | Submit a passive result to a channel                |
| `Reload(module)`                                             | Ask the agent to reload a module (empty for all)    |
| `ExpandPath(path)`                                           | Expand `${scripts}`, `${base-path}`, ...            |

### `$nscp.Settings`

```powershell
$port = $nscp.Settings.GetInt('/settings/WEB/server', 'port', 8443)
foreach ($key in $nscp.Settings.GetSection('/settings/powershell/scripts')) { $nscp.Debug("script: $key") }
```

| Method                                                                   | What it does                              |
|--------------------------------------------------------------------------|-------------------------------------------|
| `GetSection(path)`                                                        | The keys under a path                     |
| `GetString/SetString(path, key[, value])`                                 | A string value                            |
| `GetBool/SetBool(path, key[, value])`                                     | A boolean value                           |
| `GetInt/SetInt(path, key[, value])`                                       | An integer value                          |
| `RegisterPath(path, title, description, advanced)`                        | Describe a path, so it shows up in the UI |
| `RegisterKey(path, key, title, description, default, advanced)`           | Describe a key                            |
| `Save()`                                                                  | Write the settings back                   |
| `ExpandPath(path)`                                                        | Expand `${scripts}`, `${base-path}`, ...  |

A settings *value* is returned as it is configured, so a path value still reads `${scripts}/...`; pass it
through `ExpandPath` before using it.

## Installing and running

The Windows installer carries the module under the **PowerShell script support** feature; the Linux packages
ship it when they were built with the dotnet SDK. Neither ships the .NET runtime or PowerShell.

```ini
[/modules]
PowerShellScript = enabled

[/settings/powershell]
script path = ${scripts}

[/settings/powershell/scripts]
hello = hello.ps1
```

A script is looked for as configured (an absolute path is used as it stands), then under `script path`, then
under a `powershell` folder below it; the `.ps1` extension may be left off. An entry with no value is a script
named after its alias.

The module's own commands:

```
nscp powershell list                          # the loaded scripts and what each one answers
nscp powershell execute --script hello.ps1    # run a script's main function
nscp powershell help
```

### Execution policy

On Windows the machine's PowerShell execution policy applies to a script the agent loads, exactly as it would
to one you run yourself. The default on Windows Server (`RemoteSigned`) runs a local `.ps1` without further
ado; `Restricted` - the client default - refuses it, and the module says so in the log. Set
`execution policy` under `[/settings/powershell]` to run the scripts under a policy of their own:

```ini
[/settings/powershell]
execution policy = RemoteSigned
```

Anything the policy would reject is code you configured the agent to run as a service account, so treat
`Bypass` the way you would treat adding a command to `[/settings/external scripts/scripts]`.

## Notes

- One .NET runtime per process, shared with the `DotnetPlugins` module: whichever module starts first decides
  the version, and `RollForward` lets it run on any newer major version.
- The engine cannot be unloaded once it is loaded. A module reload closes the runspaces and opens new ones, but
  a background job a script left running survives it - stop what you started in `on_start`.
- A script is not handed the agent's log entries. Writing to the log from a handler that is itself fed the log
  would not end well, and a call into a runspace per log line costs more than it is worth.
- Nothing is loaded on an agent whose `[/settings/powershell/scripts]` is empty: the module registers its
  settings and stops there, so enabling it costs nothing until you configure a script.
