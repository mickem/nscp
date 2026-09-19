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
