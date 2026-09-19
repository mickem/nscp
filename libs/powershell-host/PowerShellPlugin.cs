// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

using System;
using System.Collections.Generic;
using Google.Protobuf;
using NSCP.Core;
using NSCP.Helpers;

namespace NSCP.PowerShell
{
    /// <summary>
    /// Entry point of the PowerShell script host. The PowerShellScript module
    /// loads this assembly through the managed plugin API and asks for the
    /// plugin; everything below it is ordinary .NET plugin machinery.
    /// </summary>
    public class PluginFactory : IPluginFactory
    {
        public IPlugin create(ICore core, PluginInstance instance)
        {
            // Point the assembly resolver at the PowerShell installation before
            // anything else: from here on any type from the engine may be
            // touched, and the resolver has to be in place by then.
            var log = new LogHelper(core);
            var settings = new SettingsHelper(core, instance.PluginID);
            var home = core.expandPath(settings.getString("/settings/" + instance.Alias, "powershell path", string.Empty));
            PowerShellRuntime.Configure(home, message => log.debug(message));
            return new PowerShellPlugin(core, instance);
        }
    }

    /// <summary>
    /// The script host itself: loads the configured scripts on
    /// <see cref="load"/> and routes what they registered back into them.
    /// </summary>
    public class PowerShellPlugin : IPlugin, IQueryHandler, ISubmissionHandler, IExecutionHandler
    {
        private readonly ICore core_;
        private readonly PluginInstance instance_;
        private readonly LogHelper log_;
        private ScriptManager scripts_;

        internal PowerShellPlugin(ICore core, PluginInstance instance)
        {
            core_ = core;
            instance_ = instance;
            log_ = new LogHelper(core);
        }

        public bool load(int mode)
        {
            var scripts = new ScriptManager(core_, instance_);
            // The module's own command-line commands. Registering them is what
            // routes `nscp powershell list` here; the core never sees them
            // (command-line commands are the hosting module's business), so
            // they cannot collide with another module's.
            var registry = new RegistryHelper(core_, instance_.PluginID);
            foreach (var command in Cli.Commands) registry.registerExecCommand(command, "PowerShell scripts: " + command);
            try
            {
                if (!scripts.Load())
                {
                    scripts.Dispose();
                    return false;
                }
            }
            catch (Exception e)
            {
                log_.error("Failed to load the PowerShell scripts: " + e);
                scripts.Dispose();
                return false;
            }
            scripts_ = scripts;
            return true;
        }

        public bool unload()
        {
            var scripts = scripts_;
            scripts_ = null;
            scripts?.Dispose();
            return true;
        }

        public string getName()
        {
            return "PowerShell";
        }

        public string getDescription()
        {
            return "Runs PowerShell scripts inside NSClient++";
        }

        public PluginVersion getVersion()
        {
            var version = typeof(PowerShellPlugin).Assembly.GetName().Version;
            return version == null ? new PluginVersion(0, 0, 0) : new PluginVersion(version.Major, version.Minor, version.Build);
        }

        public IQueryHandler getQueryHandler()
        {
            return this;
        }

        public ISubmissionHandler getSubmissionHandler()
        {
            return this;
        }

        // Log entries are not forwarded to scripts: a script that logs would
        // feed itself, and a per-line call into a runspace would cost more than
        // it is worth.
        public IMessageHandler getMessageHandler()
        {
            return null;
        }

        public IExecutionHandler getExecutionHandler()
        {
            return this;
        }

        public bool isActive()
        {
            return scripts_ != null;
        }

        public Result onQuery(string command, byte[] request)
        {
            var scripts = scripts_;
            if (scripts == null) return new Result();
            var reply = scripts.Query(command, request);
            return reply == null ? new Result() : new Result(reply);
        }

        public Result onSubmission(string channel, byte[] request)
        {
            var scripts = scripts_;
            if (scripts == null) return new Result();
            var reply = scripts.Submit(channel, request);
            return reply == null ? new Result() : new Result(reply);
        }

        public Result onCommand(string target, string command, byte[] request)
        {
            var scripts = scripts_;
            if (scripts == null) return new Result();
            return new Result(Cli.Run(scripts, command, request));
        }
    }

    /// <summary>
    /// `nscp powershell <command>`: the module's own sub commands, plus
    /// whatever the scripts registered with
    /// <c>$nscp.Registry.SimpleCmdline</c>.
    /// </summary>
    internal static class Cli
    {
        internal static readonly string[] Commands = { "help", "list", "execute", "run" };

        private const string Usage =
            "Usage: nscp powershell [help|list|execute]\n" +
            "  help                       Show this text\n" +
            "  list                       List the loaded scripts and what they answer\n" +
            "  execute --script <file>    Run a script's main function and print what it returned\n" +
            "    [arguments...]           Everything after the script is passed to main";

        /// <summary>
        /// Run a sub command and return the serialized ExecuteResponseMessage.
        /// A command a script registered wins over nothing else: the module's
        /// own names are checked first, so a script cannot shadow `list`.
        /// </summary>
        internal static byte[] Run(ScriptManager scripts, string command, byte[] request)
        {
            switch ((command ?? string.Empty).ToLowerInvariant())
            {
                case "":
                case "help":
                    return Reply(command, 0, Usage);
                case "list":
                    return Reply(command, 0, scripts.Describe());
                case "execute":
                case "run":
                    return Execute(scripts, command, request);
                default:
                    var reply = scripts.Command(command, request);
                    return reply ?? Reply(command, 3, "Unknown command: " + command + "\n" + Usage);
            }
        }

        private static byte[] Execute(ScriptManager scripts, string command, byte[] request)
        {
            var arguments = new List<string>();
            var message = PB.Commands.ExecuteRequestMessage.Parser.ParseFrom(request ?? Array.Empty<byte>());
            if (message.Payload.Count > 0) arguments.AddRange(message.Payload[0].Arguments);
            var file = string.Empty;
            var rest = new List<string>();
            for (var i = 0; i < arguments.Count; i++)
            {
                var argument = arguments[i];
                if ((argument == "--script" || argument == "--file") && i + 1 < arguments.Count)
                {
                    file = arguments[++i];
                    continue;
                }
                if (file.Length == 0 && !argument.StartsWith("-", StringComparison.Ordinal))
                {
                    file = argument;
                    continue;
                }
                rest.Add(argument);
            }
            if (file.Length == 0) return Reply(command, 3, "No script given.\n" + Usage);
            var text = scripts.Execute(file, rest.ToArray(), out var code);
            return Reply(command, code, text);
        }

        /// <summary>A serialized ExecuteResponseMessage carrying one answer.</summary>
        internal static byte[] Reply(string command, int code, string message)
        {
            var reply = new PB.Commands.ExecuteResponseMessage();
            reply.Payload.Add(new PB.Commands.ExecuteResponseMessage.Types.Response
            {
                Command = command ?? string.Empty,
                Result = NagiosStatus.Code(code),
                Message = message ?? string.Empty,
            });
            return reply.ToByteArray();
        }
    }
}
