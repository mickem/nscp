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
