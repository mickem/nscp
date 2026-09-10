# DotnetPlugins

Loads and hosts plugins written for .NET (C#, F#, ...)

The module does not carry a .NET runtime of its own. When it loads it locates an installed runtime through
`hostfxr` (the runtime's native hosting API), starts it inside the NSClient++ process and loads the managed
plugin API `NSCP.Core.dll` shipped in `modules/dotnet`. Each configured plugin assembly is then loaded into its
own assembly load context, its factory class is instantiated and the plugin is started. Queries, command-line
commands and passive results (channels) a plugin registers are routed to it, and it can subscribe to the log
stream. See [Extending NSClient++ with .NET](../../extending/dotnet.md) for how to write a plugin.

#### Requirements

- A .NET runtime, version 8.0 or newer, built for the same CPU architecture as NSClient++ (a 32-bit agent needs
  the x86 runtime). Only the runtime is needed to *run* plugins; the SDK is a build-time requirement.
  The module searches, in the order the .NET host itself uses: the `runtime path` setting, `DOTNET_ROOT_<ARCH>`
  (`DOTNET_ROOT_X64`, `DOTNET_ROOT_X86`, ...), `DOTNET_ROOT`, the registered install location (the Windows
  registry, or `/etc/dotnet/install_location` on Linux), the default install folders (`%ProgramFiles%\dotnet`,
  `%LOCALAPPDATA%\Microsoft\dotnet`; `/usr/lib/dotnet`, `/usr/share/dotnet`, `~/.dotnet`, ...) and, last, the
  folder of the `dotnet` launcher on `PATH`. Runtimes built for another architecture are skipped.
- The managed plugin API in `modules/dotnet` (`NSCP.Core.dll`, `NSCP.Core.runtimeconfig.json`,
  `NSCP.Core.deps.json`, `Google.Protobuf.dll`). It is part of the ".NET plugin support" installer feature on
  Windows and of the packages on Linux when NSClient++ was built with the dotnet SDK available.

#### Configuring plugins

One key per plugin under `[/settings/dotnet/plugins]`: `<alias> = <assembly>`. The value is a file in the
plugin path (tried as written first, then with `.dll` appended), an absolute path, `enabled` to load an assembly
named after the alias, or `disabled` to skip it.

```ini
[/settings/dotnet/plugins]
; modules/dotnet/NSCP.Plugin.CSharpSample.dll (the sample built from the source
; tree and copied here; it is not part of the installers), factory NSCP.Plugin.PluginFactory
NSCP.Plugin.CSharpSample = enabled
; an alias for a plugin with a different file name
inventory = Contoso.Inventory.dll
; a plugin installed elsewhere
audit = /opt/contoso/audit/Contoso.Audit.dll
; kept in the configuration but not loaded
legacy = disabled

; per-plugin override of the factory class
[/settings/dotnet/plugins/inventory]
factory class = Contoso.Inventory.Factory
```

#### Troubleshooting

| Message                                                        | Meaning                                                                                                                                         |
|----------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------|
| `No x64 .NET runtime found (looked for host/fxr/<version>/... under: ...)` | No runtime for this architecture in any of the searched roots. Install the .NET runtime or set `runtime path`.                       |
| `The managed plugin API is missing: .../NSCP.Core.runtimeconfig.json not found` | The `modules/dotnet` folder is not installed (installer feature not selected, or the build had no dotnet SDK).                    |
| `FrameworkMissingFailure`                                      | A runtime is installed but it is older than the 8.0 the API targets.                                                                            |
| `Plugin <alias> not found: <path>`                             | The configured assembly file does not exist; check `plugin path` and the value of the key.                                                     |
| `Factory class <name> not found in <assembly>`                 | The assembly has no type with that name; set `factory class` for the plugin.                                                                    |

`nscp client --module DotnetPlugins --boot --exec list` prints the plugins the module loaded.


## Enable module

To enable this module and allow using the commands you need to add `DotnetPlugins = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
DotnetPlugins = enabled
```

## Samples

_Feel free to add more samples [on this page](https://github.com/mickem/nscp/blob/master/docs/samples/DotnetPlugins_samples.md)_

The C# sample plugin in the source tree (`modules/CSharpSamplePlugin`) registers a `check_dotnet` query, a
`dotnet_hello` command and the `dotnet_sample` channel:

```
nscp client --module DotnetPlugins --boot --query check_dotnet
Hello from C#

nscp client --module DotnetPlugins --boot --exec dotnet_hello
L harpSample Received submission on dotnet_sample: check_dotnet Ok Hello from C#
Hello exec from C# (log entries seen: 0, submit: Received by C#)
```

The `L harpSample` prefix is the console logger's rendering of the log line the plugin wrote when the passive
result reached its submission handler: the level (`L` for info) and the last ten characters of the sender, here the
plugin alias `NSCP.Plugin.CSharpSample`.



## Configuration

| Path / Section                            | Description    |
|-------------------------------------------|----------------|
| [/settings/dotnet](#dotnet-plugins)       | DOTNET PLUGINS |
| [/settings/dotnet/plugins](#.net-plugins) | .NET plugins   |


### DOTNET PLUGINS <a id="/settings/dotnet"></a>

Section for the DotnetPlugins module: hosts modules written for .NET.

| Key                                     | Default Value             | Description           |
|-----------------------------------------|---------------------------|-----------------------|
| [factory class](#default-factory-class) | NSCP.Plugin.PluginFactory | Default factory class |
| [plugin path](#plugin-path)             | ${module-path}/dotnet     | Plugin path           |
| [runtime path](#.net-runtime-root)      |                           | .NET runtime root     |


```ini
# Section for the DotnetPlugins module: hosts modules written for .NET.
[/settings/dotnet]
factory class=NSCP.Plugin.PluginFactory
plugin path=${module-path}/dotnet
```

#### Default factory class <a id="/settings/dotnet/factory class"></a>

Fully qualified name of the IPluginFactory implementation instantiated in a plugin assembly unless the plugin overrides it.


| Key            | Description                           |
|----------------|---------------------------------------|
| Path:          | [/settings/dotnet](#/settings/dotnet) |
| Key:           | factory class                         |
| Advanced:      | Yes (means it is not commonly used)   |
| Default value: | `NSCP.Plugin.PluginFactory`           |


**Sample:**

```
[/settings/dotnet]
# Default factory class
factory class=NSCP.Plugin.PluginFactory
```

#### Plugin path <a id="/settings/dotnet/plugin path"></a>

Folder holding NSCP.Core.dll (the managed plugin API shipped with NSClient++) and the .NET plugin assemblies. Falls back to ${exe-path}/modules/dotnet when the folder does not exist.


| Key            | Description                           |
|----------------|---------------------------------------|
| Path:          | [/settings/dotnet](#/settings/dotnet) |
| Key:           | plugin path                           |
| Default value: | `${module-path}/dotnet`               |


**Sample:**

```
[/settings/dotnet]
# Plugin path
plugin path=${module-path}/dotnet
```

#### .NET runtime root <a id="/settings/dotnet/runtime path"></a>

Root folder of the .NET installation to host (the folder containing 'host/fxr'). Leave empty to use DOTNET_ROOT, the registered install location or the platform's default install folders.


| Key            | Description                           |
|----------------|---------------------------------------|
| Path:          | [/settings/dotnet](#/settings/dotnet) |
| Key:           | runtime path                          |
| Advanced:      | Yes (means it is not commonly used)   |
| Default value: | _N/A_                                 |


**Sample:**

```
[/settings/dotnet]
# .NET runtime root
runtime path=
```

### .NET plugins <a id="/settings/dotnet/plugins"></a>

Plugins to load: <alias> = <assembly> where <assembly> is a file in the plugin path (the .dll extension is optional), an absolute path, 'enabled' to load an assembly named after the alias, or 'disabled' to skip it. Set the factory class for one plugin under plugins/<alias> with the key 'factory class'.


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.





