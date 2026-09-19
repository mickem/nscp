// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

using System;
using System.Collections.Generic;
using System.IO;
using Google.Protobuf;
using NSCP.Core;
using NSCP.Helpers;

namespace NSCP.PowerShell
{
    /// <summary>
    /// The scripts, what each of them answers, and the routing of a query,
    /// submission or command-line command into the right one.
    /// </summary>
    internal sealed class ScriptManager : IDisposable
    {
        private sealed class Handler
        {
            internal ScriptRunspace Script;
            internal object Callback;
            internal string Description;
        }

        private readonly ICore core_;
        private readonly PluginInstance instance_;
        private readonly LogHelper log_;
        private readonly SettingsHelper settings_;
        private readonly RegistryHelper registry_;
        private readonly string settingsPath_;

        private readonly object lock_ = new object();
        private readonly List<ScriptRunspace> scripts_ = new List<ScriptRunspace>();
        private readonly Dictionary<string, Handler> queries_ = new Dictionary<string, Handler>(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, Handler> commands_ = new Dictionary<string, Handler>(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, Handler> channels_ = new Dictionary<string, Handler>(StringComparer.OrdinalIgnoreCase);

        private string scriptRoot_ = string.Empty;
        private string executionPolicy_ = string.Empty;

        internal ScriptManager(ICore core, PluginInstance instance)
        {
            core_ = core;
            instance_ = instance;
            log_ = new LogHelper(core);
            settings_ = new SettingsHelper(core, instance.PluginID);
            registry_ = new RegistryHelper(core, instance.PluginID);
            // The native module registered its settings under the alias it was
            // loaded with and hands us the same one, so both halves read the
            // same section whatever the module is aliased to.
            settingsPath_ = "/settings/" + instance.Alias;
        }

        /// <summary>Read the settings, find the scripts and run each of them.</summary>
        internal bool Load()
        {
            scriptRoot_ = core_.expandPath(settings_.getString(settingsPath_, "script path", "${scripts}"));
            executionPolicy_ = settings_.getString(settingsPath_, "execution policy", string.Empty);

            var section = settingsPath_ + "/scripts";
            var aliases = settings_.getKeys(section);
            if (aliases.Count == 0)
            {
                log_.debug("No PowerShell scripts configured under " + section);
                return true;
            }
            if (string.IsNullOrEmpty(PowerShellRuntime.Home))
            {
                log_.error("No PowerShell 7 installation found (looked in: " + PowerShellRuntime.Searched +
                           "). Install PowerShell 7 or set 'powershell path' in " + settingsPath_ +
                           ". Windows PowerShell 5.1 cannot be used: it is built on the .NET Framework.");
                return false;
            }

            foreach (var alias in aliases)
            {
                var configured = settings_.getString(section, alias, string.Empty);
                var path = Resolve(alias, configured);
                if (path == null)
                {
                    log_.error("PowerShell script not found: " + (configured.Length > 0 ? configured : alias) + " (looked under " + scriptRoot_ + ")");
                    continue;
                }
                Add(alias, path);
            }

            // Everything is registered by now, so a script that wants to do
            // something once the agent is up gets its turn.
            foreach (var script in Snapshot())
            {
                if (!script.HasFunction("on_start")) continue;
                Report(script, "on_start", script.Invoke("on_start", Array.Empty<object>()));
            }
            return true;
        }

        /// <summary>Load one script into its own runspace.</summary>
        private void Add(string alias, string path)
        {
            var script = new ScriptRunspace(alias, path);
            var api = new NscpApi(core_, instance_, this, alias, path);
            lock (lock_) scripts_.Add(script);
            ScriptOutput output;
            try
            {
                output = script.Open(api, Path.GetDirectoryName(path), executionPolicy_);
            }
            catch (Exception e)
            {
                log_.error("Failed to load PowerShell script " + path + ": " + e.Message);
                return;
            }
            Report(script, "load", output);
            log_.debug("Loaded PowerShell script " + alias + ": " + path);
        }

        /// <summary>
        /// Resolve a configured script to a file: as configured when that is an
        /// absolute path or exists as given, then under the script path, then
        /// under a "powershell" folder below it. An empty value means the
        /// script is named after its alias.
        /// </summary>
        internal string Resolve(string alias, string configured)
        {
            var file = string.IsNullOrWhiteSpace(configured) ? alias : configured.Trim();
            foreach (var candidate in Candidates(file))
            {
                if (File.Exists(candidate)) return Path.GetFullPath(candidate);
            }
            return null;
        }

        private IEnumerable<string> Candidates(string file)
        {
            var named = new List<string> { file };
            if (!file.EndsWith(".ps1", StringComparison.OrdinalIgnoreCase)) named.Add(file + ".ps1");
            foreach (var name in named)
            {
                if (Path.IsPathRooted(name))
                {
                    yield return name;
                    continue;
                }
                yield return name;
                if (scriptRoot_.Length == 0) continue;
                yield return Path.Combine(scriptRoot_, name);
                yield return Path.Combine(scriptRoot_, "powershell", name);
            }
        }

        // -- registration, called from a script while it loads ----------------

        internal bool RegisterQuery(string script, string command, string description, object handler)
        {
            if (!Remember(queries_, script, command, description, handler)) return false;
            return registry_.registerCommand(command, description ?? string.Empty);
        }

        internal bool RegisterCommand(string script, string command, object handler)
        {
            if (!Remember(commands_, script, command, string.Empty, handler)) return false;
            return registry_.registerExecCommand(command, string.Empty);
        }

        internal bool RegisterChannel(string script, string channel, object handler)
        {
            if (!Remember(channels_, script, channel, string.Empty, handler)) return false;
            return registry_.registerChannel(channel);
        }

        private bool Remember(Dictionary<string, Handler> into, string script, string name, string description, object handler)
        {
            if (string.IsNullOrWhiteSpace(name) || handler == null) return false;
            lock (lock_)
            {
                var owner = scripts_.Find(s => string.Equals(s.Alias, script, StringComparison.Ordinal));
                if (owner == null) return false;
                into[name] = new Handler { Script = owner, Callback = handler, Description = description ?? string.Empty };
            }
            return true;
        }

        // -- routing ----------------------------------------------------------

        /// <summary>Answer a check, or null when no script registered it.</summary>
        internal byte[] Query(string command, byte[] request)
        {
            var handler = Find(queries_, command);
            if (handler == null) return null;
            var arguments = new List<string>();
            var message = PB.Commands.QueryRequestMessage.Parser.ParseFrom(request ?? Array.Empty<byte>());
            if (message.Payload.Count > 0) arguments.AddRange(message.Payload[0].Arguments);

            var output = handler.Script.Invoke(handler.Callback, new object[] { command, arguments.ToArray() });
            int code;
            string text;
            string perf;
            if (output.Failed)
            {
                code = 3;
                text = "Error in " + handler.Script.Alias + ": " + output.ErrorText;
                perf = string.Empty;
                log_.error("PowerShell check " + command + " failed in " + handler.Script.Path + ": " + output.ErrorText);
            }
            else
            {
                ScriptResult.Read(output, out code, out text, out perf);
                foreach (var line in output.Information) log_.debug(handler.Script.Alias + ": " + line);
            }

            var reply = new PB.Commands.QueryResponseMessage();
            var response = new PB.Commands.QueryResponseMessage.Types.Response { Command = command, Result = NagiosStatus.Code(code) };
            // The native module splits "message|perfdata" and parses the perf
            // data with the same parser the external-script path uses.
            response.Lines.Add(new PB.Commands.QueryResponseMessage.Types.Response.Types.Line
            {
                Message = perf.Length > 0 ? text + "|" + perf : text,
            });
            reply.Payload.Add(response);
            return reply.ToByteArray();
        }

        /// <summary>Hand a passive result to the script that subscribed, or null.</summary>
        internal byte[] Submit(string channel, byte[] request)
        {
            var handler = Find(channels_, channel);
            if (handler == null) return null;
            var message = PB.Commands.SubmitRequestMessage.Parser.ParseFrom(request ?? Array.Empty<byte>());
            var command = string.Empty;
            var code = 3;
            var text = string.Empty;
            var perf = string.Empty;
            if (message.Payload.Count > 0)
            {
                var payload = message.Payload[0];
                command = payload.Command ?? string.Empty;
                code = (int)payload.Result;
                if (payload.Lines.Count > 0)
                {
                    text = payload.Lines[0].Message ?? string.Empty;
                    perf = PerfData.Format(payload.Lines[0]);
                }
            }

            var output = handler.Script.Invoke(handler.Callback, new object[] { channel, command, NagiosStatus.Name(code), text, perf });
            var reply = new PB.Commands.SubmitResponseMessage();
            var response = new PB.Commands.SubmitResponseMessage.Types.Response { Command = command };
            if (output.Failed)
            {
                log_.error("PowerShell channel " + channel + " failed in " + handler.Script.Path + ": " + output.ErrorText);
                response.Result = new PB.Common.Result
                {
                    Code = PB.Common.Result.Types.StatusCodeType.StatusError,
                    Message = output.ErrorText,
                };
            }
            else
            {
                response.Result = new PB.Common.Result { Code = PB.Common.Result.Types.StatusCodeType.StatusOk };
            }
            reply.Payload.Add(response);
            return reply.ToByteArray();
        }

        /// <summary>Run a command-line command a script registered, or null.</summary>
        internal byte[] Command(string command, byte[] request)
        {
            var handler = Find(commands_, command);
            if (handler == null) return null;
            var arguments = new List<string>();
            var message = PB.Commands.ExecuteRequestMessage.Parser.ParseFrom(request ?? Array.Empty<byte>());
            if (message.Payload.Count > 0) arguments.AddRange(message.Payload[0].Arguments);

            var output = handler.Script.Invoke(handler.Callback, new object[] { command, arguments.ToArray() });
            if (output.Failed) return Cli.Reply(command, 3, "Error in " + handler.Script.Alias + ": " + output.ErrorText);
            ScriptResult.Read(output, out var code, out var text, out _);
            if (output.Information.Count > 0) text = string.Join("\n", output.Information) + (text.Length > 0 ? "\n" + text : string.Empty);
            return Cli.Reply(command, code, text);
        }

        /// <summary>The loaded scripts and what each of them answers, for `list`.</summary>
        internal string Describe()
        {
            lock (lock_)
            {
                if (scripts_.Count == 0) return "No PowerShell scripts loaded";
                var text = new List<string> { "Loaded PowerShell scripts:" };
                foreach (var script in scripts_)
                {
                    text.Add("  " + script.Alias + ": " + script.Path);
                    foreach (var entry in queries_)
                    {
                        if (entry.Value.Script != script) continue;
                        text.Add("    query " + entry.Key + (entry.Value.Description.Length > 0 ? " - " + entry.Value.Description : string.Empty));
                    }
                    foreach (var entry in commands_)
                    {
                        if (entry.Value.Script == script) text.Add("    command " + entry.Key);
                    }
                    foreach (var entry in channels_)
                    {
                        if (entry.Value.Script == script) text.Add("    channel " + entry.Key);
                    }
                }
                return string.Join("\n", text);
            }
        }

        /// <summary>
        /// Run one script outside the configured set, for
        /// `nscp powershell execute`. It gets its own runspace, which is closed
        /// again afterwards, so it cannot disturb the loaded scripts.
        /// </summary>
        internal string Execute(string file, string[] arguments, out int code)
        {
            code = 3;
            var path = Resolve(file, file);
            if (path == null) return "Script not found: " + file + " (looked under " + scriptRoot_ + ")";
            if (string.IsNullOrEmpty(PowerShellRuntime.Home)) return "No PowerShell 7 installation found (looked in: " + PowerShellRuntime.Searched + ")";
            using (var script = new ScriptRunspace(file, path))
            {
                var api = new NscpApi(core_, instance_, this, file, path);
                var output = script.Open(api, Path.GetDirectoryName(path), executionPolicy_);
                if (output.Failed) return output.ErrorText;
                var text = new List<string>(output.Information);
                if (script.HasFunction("main"))
                {
                    output = script.Invoke("main", new object[] { arguments ?? Array.Empty<string>() });
                    if (output.Failed) return output.ErrorText;
                    text.AddRange(output.Information);
                }
                ScriptResult.Read(output, out code, out var result, out _);
                if (result.Length > 0) text.Add(result);
                return string.Join("\n", text);
            }
        }

        public void Dispose()
        {
            List<ScriptRunspace> scripts;
            lock (lock_)
            {
                scripts = new List<ScriptRunspace>(scripts_);
                scripts_.Clear();
                queries_.Clear();
                commands_.Clear();
                channels_.Clear();
            }
            foreach (var script in scripts) script.Dispose();
        }

        private Handler Find(Dictionary<string, Handler> into, string name)
        {
            lock (lock_) return into.TryGetValue(name ?? string.Empty, out var handler) ? handler : null;
        }

        private List<ScriptRunspace> Snapshot()
        {
            lock (lock_) return new List<ScriptRunspace>(scripts_);
        }

        /// <summary>Log whatever a script wrote while loading or starting.</summary>
        private void Report(ScriptRunspace script, string what, ScriptOutput output)
        {
            foreach (var line in output.Information) log_.debug(script.Alias + ": " + line);
            if (!output.Failed) return;
            log_.error("PowerShell script " + script.Path + " failed to " + what + ": " + output.ErrorText);
        }
    }
}
