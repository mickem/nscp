# DotnetPlugins

Loads and hosts plugins written for .NET (C#, F#, ...) inside NSClient++, on Windows and Linux alike.

The module does not carry a .NET runtime of its own. When it loads it locates an installed runtime through
`hostfxr` (the runtime's native hosting API), starts it inside the NSClient++ process and loads the managed
plugin API `NSCP.Core.dll` shipped in `modules/dotnet`. Each configured plugin assembly is then loaded into its
own assembly load context, its factory class is instantiated and the plugin is started. Queries for the commands
a plugin registers are routed to it.

## Requirements

- A .NET runtime, version 8.0 or newer (the runtime only; the SDK is not needed to *run* plugins).
  The module finds it through, in order: the `runtime path` setting, the `DOTNET_ROOT` environment variable, the
  `dotnet` launcher on `PATH`, the Windows registry / `%ProgramFiles%\dotnet`, and the usual Linux install
  locations (`/usr/lib/dotnet`, `/usr/share/dotnet`, `~/.dotnet`, ...).
- The managed plugin API in `modules/dotnet` (`NSCP.Core.dll`, `NSCP.Core.runtimeconfig.json`,
  `NSCP.Core.deps.json`, `Google.Protobuf.dll`). It is part of the ".NET plugin support" installer feature on
  Windows and of the packages on Linux when NSClient++ was built with the dotnet SDK available.

## Enable module

To enable this module add `DotnetPlugins = enabled` to the `[/modules]` section in nsclient.ini:

```ini
[/modules]
DotnetPlugins = enabled
```

## Configuration

| Path / Section                                   | Description                                   |
|--------------------------------------------------|-----------------------------------------------|
| [/settings/dotnet](#dotnet-plugins)              | Runtime location, plugin folder, factory class |
| [/settings/dotnet/plugins](#net-plugins)         | The plugins to load                            |

### DOTNET PLUGINS <a id="/settings/dotnet"></a>

| Key           | Default value                | Description                                                                                                                                           |
|---------------|------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------|
| plugin path   | `${module-path}/dotnet`      | Folder holding `NSCP.Core.dll` and the plugin assemblies. Falls back to `${exe-path}/modules/dotnet` when the folder does not exist.                   |
| runtime path  |                              | Root of the .NET installation to host (the folder containing `host/fxr`). Leave empty to auto-detect.                                                 |
| factory class | `NSCP.Plugin.PluginFactory`  | Fully qualified name of the `IPluginFactory` implementation instantiated in a plugin assembly, unless the plugin overrides it (see below).            |

### .NET plugins <a id="/settings/dotnet/plugins"></a>

One key per plugin: `<alias> = <assembly>`. The value is a file in the plugin path (the `.dll` extension is
optional), an absolute path, or `enabled` to load an assembly named after the alias.

```ini
[/settings/dotnet/plugins]
; modules/dotnet/NSCP.Plugin.CSharpSample.dll, factory NSCP.Plugin.PluginFactory
NSCP.Plugin.CSharpSample = enabled
; an alias for a plugin with a different file name
inventory = Contoso.Inventory.dll
; a plugin installed elsewhere
audit = /opt/contoso/audit/Contoso.Audit.dll

; per-plugin override of the factory class
[/settings/dotnet/plugins/inventory]
factory class = Contoso.Inventory.Factory
```

## Sample

The C# sample plugin in the source tree (`modules/CSharpSamplePlugin`) registers a `check_dotnet` command:

```
nscp client --module DotnetPlugins --boot --query check_dotnet
Hello from C#
```

## Troubleshooting

| Message                                                        | Meaning                                                                                                                                         |
|----------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------|
| `No .NET runtime found (looked for host/fxr/<version>/... under: ...)` | No runtime in any of the searched roots. Install the .NET runtime or set `runtime path`.                                                  |
| `The managed plugin API is missing: .../NSCP.Core.runtimeconfig.json not found` | The `modules/dotnet` folder is not installed (installer feature not selected, or the build had no dotnet SDK).                    |
| `FrameworkMissingFailure`                                      | A runtime is installed but it is older than the 8.0 the API targets.                                                                            |
| `Plugin <alias> not found: <path>`                             | The configured assembly file does not exist; check `plugin path` and the value of the key.                                                     |
| `Factory class <name> not found in <assembly>`                 | The assembly has no type with that name; set `factory class` for the plugin.                                                                    |

See [Extending NSClient++ with .NET](../../extending/dotnet.md) for how to write a plugin.
