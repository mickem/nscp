# Extending NSClient++

NSClient++ can be extended in multiple ways.
The easy choice is adding external scripts which are simply scripts which are executed as is by NSClient++ but if you do not fancy easy choices you can also go hard-core and write your own custom Modules in C++. Here I will list the various options and some simple pros and cons.


## External scripts

External scripts are script you write in any language of choice.
They are executed exactly like regular commands on the host operating system.
The script has to conform to the nagios plugin guidelines.

**Pros:**

- Very easy to write your own script in your favorite scripting language.
- Powerful in that your script can do whatever you want

**Cons:**

- Stateless, unless you implement the state your self.
- Forking overhead as the script is started afress each time it is executed
- No background threads.
- Only supported by check_commands

## Lua Scripts

Script written in the Lua scripting language.
These script run inside NSClient++ and can interact with NSClient++.

**Pros:**

- Very Powerful in that your script can do whatever you want and you can interact with NSClient++.
- Supports all features of NSclient++, metrics, commands, queries, passive results as well as events.
- Stateful as the scripts run in the background
- No forking overhead

**Cons:**

- Lua might be considered a slightly arcane language in the monitor space.
- Adding third party modules requires compiling modules.
- No background threads.

## Python Scripts

Script written in the Python scripting language.
These script run inside NSClient++ and can interact with NSClient++.

**Pros:**

- Very Powerful in that your script can do whatever you want and you can interact with NSClient++.
- Supports all features of NSclient++, metrics, commands, queries, passive results as well as events.
- Stateful as the scripts run in the background
- No forking overhead
- Background threads.

**Cons:**

- Slightly awkward to distribute third party modules.

## PowerShell Scripts

Scripts written in PowerShell, run inside NSClient++ by the PowerShellScript module rather than by
starting `pwsh` for every check. See [PowerShell scripts](powershell.md).

**Pros:**

- Very Powerful in that your script can do whatever you want and you can interact with NSClient++.
- Supports queries, command-line commands, passive results, settings and logging.
- Stateful as the scripts run in the background: modules, sessions and connections are set up once.
- No forking overhead, and none of the quoting pain of the `ps1` external-script wrapping.

**Cons:**

- Requires a .NET runtime (8.0 or newer) and PowerShell 7 on the monitored machine; Windows PowerShell 5.1
  cannot be used.
- One script answers its checks one at a time (its runspace is shared).

## .NET plugins

Plugins written for .NET (C#, F#, ...) hosted in-process by the DotnetPlugins module, on Windows and Linux.
See [.NET plugins](dotnet.md).

**Pros:**

- Very Powerful in that your plugin can do whatever you want and you can interact with NSClient++.
- Supports queries, commands, passive results, settings and logging through the same protocol as native modules.
- Stateful as the plugins run in the background, with full access to the .NET ecosystem (NuGet, threads, ...).
- No forking overhead.

**Cons:**

- Requires a .NET runtime (8.0 or newer) on the monitored machine.
- Compiled, so modifications require a rebuild.

## C/C++ modules

This is how all the built-in functionality is written.

**Pros:**

- Very Powerful in that your script can do whatever you want and you can interact with NSClient++.
- Supports all features of NSclient++, metrics, commands, queries, passive results as well as events.
- Stateful as the scripts run in the background
- No forking overhead
- Background threads.

**Cons:**

- Requires you to compile the modules meaning modifications are hard.


## Zip modules

This is new in 0.5.2 and a way to package scripts and configuration inside a zip file fo easy distribution in NSClient++.

**Pros:**

- Powerful: While not scripting it allows you to add both External scripts and python scripts.
- Supports all features of NSclient++, metrics, commands, queries, passive results as well as events.
- Stateful as the scripts run in the background
- No forking overhead (depending on which scripting technology you use)
- Background threads.

**Cons:**

- Configuration is currently a bit limited.
