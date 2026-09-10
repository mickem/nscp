---
icon: "🔌"
modules: [DotnetPlugins, packaging]
action: none
---
**.NET plugins are back, on Windows and Linux.** The `DotnetPlugins` module is
built again, now hosting an installed .NET runtime (8.0 or newer) through
`hostfxr` instead of the old Windows-only C++/CLI build, so it also works on
Linux. Nothing to do on a default install: the module is not loaded unless you
add `DotnetPlugins = enabled` to `[/modules]`, and no runtime is bundled. The
Windows installer regained the ".NET plugin support" feature (selected by
default, installs `modules/DotnetPlugins.dll` and the managed API in
`modules/dotnet/`); the Linux packages ship the same files when they were built
with the dotnet SDK. Plugins are configured under `[/settings/dotnet/plugins]`
(`<alias> = <assembly>`), see the
[DotnetPlugins reference](../reference/generic/DotnetPlugins.md) and
[Extending with .NET](../extending/dotnet.md). Plugins written against the
pre-0.6 C++/CLI API (`NSCP.Core.dll` for the .NET Framework) must be rebuilt
against the new `net8.0` `NSCP.Core.dll`; the interfaces are the same.
