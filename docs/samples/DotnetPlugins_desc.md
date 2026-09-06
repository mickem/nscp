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
; modules/dotnet/NSCP.Plugin.CSharpSample.dll, factory NSCP.Plugin.PluginFactory
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
