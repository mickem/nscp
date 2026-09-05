# .NET plugins

The **DotnetPlugins** module hosts plugins written for .NET (C#, F#, VB.NET, ...) in-process, on Windows and on
Linux. A plugin talks to NSClient++ through the same protocol-buffer messages the native modules use, so it can
answer queries, run commands, submit passive results, read and write settings and log through the agent.

This page shows how to write, build and install one. The module's configuration is documented in the
[DotnetPlugins reference](../reference/generic/DotnetPlugins.md).

## How it fits together

```
nscp ──loads──> DotnetPlugins (native module)
                  │  hostfxr: starts the installed .NET runtime
                  └──loads──> modules/dotnet/NSCP.Core.dll   (managed plugin API + bridge)
                                 └──loads──> modules/dotnet/YourPlugin.dll
```

`NSCP.Core.dll` is built and shipped with NSClient++. It contains:

- the plugin contract: `NSCP.Core.IPluginFactory`, `IPlugin`, `IQueryHandler`, `ISubmissionHandler`, `ICore`, ...
- the helpers in `NSCP.Helpers`: `LogHelper`, `SettingsHelper`, `RegistryHelper`
- the protocol-buffer messages (`PB.Commands`, `PB.Settings`, `PB.Registry`, `PB.Log`, ...) generated from
  `libs/protobuf/*.proto`
- the native bridge the module calls into

## Writing a plugin

A plugin assembly needs a factory class (by default `NSCP.Plugin.PluginFactory`, configurable per plugin) with a
parameterless constructor. The module instantiates it and calls `create`, then `load` on the returned plugin once
NSClient++ has booted the module.

```csharp
using NSCP.Core;
using NSCP.Helpers;

namespace NSCP.Plugin
{
    public class PluginFactory : IPluginFactory
    {
        public IPlugin create(ICore core, PluginInstance instance) => new HelloPlugin(core, instance);
    }
}

public class HelloPlugin : IPlugin
{
    private readonly ICore core;
    private readonly PluginInstance instance;
    private readonly LogHelper log;

    public HelloPlugin(ICore core, PluginInstance instance)
    {
        this.core = core;
        this.instance = instance;
        this.log = new LogHelper(core);
    }

    public bool load(int mode)
    {
        // Register the commands this plugin answers. The plugin id identifies
        // the hosting module to NSClient++; always pass instance.PluginID.
        new RegistryHelper(core, instance.PluginID).registerCommand("check_hello", "Says hello");
        log.info("hello plugin loaded as " + instance.Alias);
        return true;
    }

    public bool unload() => true;
    public string getName() => "Hello";
    public string getDescription() => "A minimal .NET plugin";
    public PluginVersion getVersion() => new PluginVersion(1, 0, 0);

    public IQueryHandler getQueryHandler() => new HelloQueries();
    public ISubmissionHandler getSubmissionHandler() => null;
    public IMessageHandler getMessageHandler() => null;
    public IExecutionHandler getExecutionHandler() => null;
}

public class HelloQueries : IQueryHandler
{
    public bool isActive() => true;

    public Result onQuery(string command, byte[] request)
    {
        var query = PB.Commands.QueryRequestMessage.Parser.ParseFrom(request);
        var response = new PB.Commands.QueryResponseMessage.Types.Response { Command = command, Result = PB.Common.ResultCode.Ok };
        response.Lines.Add(new PB.Commands.QueryResponseMessage.Types.Response.Types.Line { Message = "Hello from .NET" });
        var reply = new PB.Commands.QueryResponseMessage();
        reply.Payload.Add(response);
        return new Result(Google.Protobuf.MessageExtensions.ToByteArray(reply));
    }
}
```

`onQuery` receives a serialized `QueryRequestMessage` holding the single request (command, arguments, target) and
returns a serialized `QueryResponseMessage`; the first payload becomes the check result (status, message lines,
performance data). The other handlers work the same way with their respective messages.

Everything a plugin needs from NSClient++ goes through `ICore`:

| Method                        | Message (request → response)                          | Use                                                       |
|-------------------------------|-------------------------------------------------------|-----------------------------------------------------------|
| `query(bytes)`                | `QueryRequestMessage` → `QueryResponseMessage`        | Run another check (any module)                            |
| `exec(target, bytes)`         | `ExecuteRequestMessage` → `ExecuteResponseMessage`    | Run a command-line style command                          |
| `submit(channel, bytes)`      | `SubmitRequestMessage` → `SubmitResponseMessage`      | Submit a passive result to a channel                      |
| `settings(bytes)`             | `SettingsRequestMessage` → `SettingsResponseMessage`  | Read / write / register settings (see `SettingsHelper`)   |
| `registry(bytes)`             | `RegistryRequestMessage` → `RegistryResponseMessage`  | Register commands (see `RegistryHelper`)                  |
| `log(bytes)`                  | `LogEntry`                                            | Log through the agent (see `LogHelper`)                   |
| `reload(module)`              |                                                       | Ask the core to reload a module                           |

## Building

Create an SDK-style class library targeting `net8.0` (or newer), reference the `NSCP.Core.dll` of an installed
agent and the `Google.Protobuf` package, and keep the plugin from copying its own `NSCP.Core.dll` next to the
shared one:

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
    <EnableDynamicLoading>true</EnableDynamicLoading>
    <RollForward>LatestMajor</RollForward>
  </PropertyGroup>
  <ItemGroup>
    <Reference Include="NSCP.Core">
      <HintPath>C:\Program Files\NSClient++\modules\dotnet\NSCP.Core.dll</HintPath>
      <Private>false</Private>
    </Reference>
    <PackageReference Include="Google.Protobuf" Version="3.36.1" ExcludeAssets="runtime" />
  </ItemGroup>
</Project>
```

`EnableDynamicLoading` makes the build emit a `<plugin>.deps.json`, which the module uses to resolve any private
dependencies of the plugin (NuGet packages it uses) from the plugin folder. Copy the build output (`YourPlugin.dll`,
`YourPlugin.deps.json` and any private dependencies) into the agent's `modules/dotnet` folder
(`C:\Program Files\NSClient++\modules\dotnet` on Windows, `/usr/lib/nsclient/modules/dotnet` on Linux).

The sample plugin in the source tree (`modules/CSharpSamplePlugin/`) is a complete, buildable example.

## Installing and running

```ini
[/modules]
DotnetPlugins = enabled

[/settings/dotnet/plugins]
YourPlugin = enabled
```

Then:

```
nscp client --module DotnetPlugins --boot --query check_hello
```

or, with the agent running, through any of the usual protocols (`check_nrpe -c check_hello`, the REST API, ...).

## Notes

- One .NET runtime per process: the first module to start a runtime decides its version; `RollForward` in
  `NSCP.Core.runtimeconfig.json` lets it run on any newer major version.
- Plugins are loaded into isolated assembly load contexts, so two plugins can use different versions of a NuGet
  package. The contract assemblies (`NSCP.Core`, `Google.Protobuf`) are always shared with the host.
- Unhandled exceptions in a plugin are caught at the boundary, logged through the agent and turned into an
  `UNKNOWN` result; they do not take the agent down.
