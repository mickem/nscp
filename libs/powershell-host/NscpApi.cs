// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

using System;
using System.Collections.Generic;
using Google.Protobuf;
using NSCP.Core;
using NSCP.Helpers;
using PB.Log;

namespace NSCP.PowerShell
{
    /// <summary>
    /// The result of a check, as a script sees it: the Nagios status plus the
    /// message and performance data.
    /// </summary>
    public sealed class NscpResult
    {
        internal NscpResult(int code, string message, string perf)
        {
            Code = code;
            Message = message ?? string.Empty;
            Perf = perf ?? string.Empty;
        }

        /// <summary>0 ok, 1 warning, 2 critical, 3 unknown.</summary>
        public int Code { get; }

        /// <summary>The status as a word: "ok", "warning", "critical" or "unknown".</summary>
        public string Status
        {
            get { return NagiosStatus.Name(Code); }
        }

        public string Message { get; }
        public string Perf { get; }

        public override string ToString()
        {
            return Perf.Length > 0 ? Status + ": " + Message + "|" + Perf : Status + ": " + Message;
        }
    }

    /// <summary>
    /// The <c>$nscp</c> object a script is handed: logging, and the three
    /// sub-objects that mirror the Lua and Python script APIs.
    /// </summary>
    public sealed class NscpApi
    {
        private readonly LogHelper log_;

        internal NscpApi(ICore core, PluginInstance instance, ScriptManager manager, string alias, string path)
        {
            log_ = new LogHelper(core);
            Core = new CoreApi(core);
            Registry = new RegistryApi(manager, alias);
            Settings = new SettingsApi(core, instance.PluginID);
            Alias = instance.Alias;
            ScriptAlias = alias;
            ScriptPath = path;
        }

        /// <summary>Run checks, commands and submissions through the agent.</summary>
        public CoreApi Core { get; }

        /// <summary>Tell the agent which checks, commands and channels this script answers.</summary>
        public RegistryApi Registry { get; }

        /// <summary>Read and write the agent's settings.</summary>
        public SettingsApi Settings { get; }

        /// <summary>The alias the PowerShellScript module is loaded under.</summary>
        public string Alias { get; }

        /// <summary>The alias this script is configured under.</summary>
        public string ScriptAlias { get; }

        /// <summary>The full path of this script.</summary>
        public string ScriptPath { get; }

        public void Info(string message) => Log("info", message);
        public void Error(string message) => Log("error", message);
        public void Debug(string message) => Log("debug", message);
        public void Warning(string message) => Log("warning", message);
        public void Critical(string message) => Log("critical", message);

        /// <summary>Write one line to the agent's log at the given level.</summary>
        public void Log(string level, string message)
        {
            log_.log(ScriptPath, 0, Level(level), message ?? string.Empty);
        }

        private static LogEntry.Types.Entry.Types.Level Level(string level)
        {
            switch ((level ?? string.Empty).ToLowerInvariant())
            {
                case "error":
                    return LogEntry.Types.Entry.Types.Level.LogError;
                case "critical":
                    return LogEntry.Types.Entry.Types.Level.LogCritical;
                case "warning":
                case "warn":
                    return LogEntry.Types.Entry.Types.Level.LogWarning;
                case "debug":
                    return LogEntry.Types.Entry.Types.Level.LogDebug;
                default:
                    return LogEntry.Types.Entry.Types.Level.LogInfo;
            }
        }
    }

    /// <summary>Nagios status words and the numbers behind them.</summary>
    internal static class NagiosStatus
    {
        internal static string Name(int code)
        {
            switch (code)
            {
                case 0:
                    return "ok";
                case 1:
                    return "warning";
                case 2:
                    return "critical";
                default:
                    return "unknown";
            }
        }

        /// <summary>
        /// Read a status out of whatever a script returned: a number, or one of
        /// the words the Lua and Python APIs use. Anything else is unknown -
        /// a typo in a status must not silently read as OK.
        /// </summary>
        internal static int Parse(object value, int fallback)
        {
            if (value == null) return fallback;
            if (value is bool flag) return flag ? 0 : 2;
            if (value is int || value is long || value is short || value is byte) return Clamp(Convert.ToInt32(value));
            var text = value.ToString().Trim().ToLowerInvariant();
            if (text.Length == 0) return fallback;
            if (int.TryParse(text, out var number)) return Clamp(number);
            switch (text)
            {
                case "ok":
                case "good":
                    return 0;
                case "warn":
                case "warning":
                    return 1;
                case "crit":
                case "critical":
                case "bad":
                    return 2;
                default:
                    return 3;
            }
        }

        private static int Clamp(int code)
        {
            return code >= 0 && code <= 3 ? code : 3;
        }

        internal static PB.Common.ResultCode Code(int code)
        {
            switch (code)
            {
                case 0:
                    return PB.Common.ResultCode.Ok;
                case 1:
                    return PB.Common.ResultCode.Warning;
                case 2:
                    return PB.Common.ResultCode.Critical;
                default:
                    return PB.Common.ResultCode.Unknown;
            }
        }
    }

    /// <summary>
    /// Calls into NSClient++: run another check, run a command-line command,
    /// submit a passive result, reload a module.
    /// </summary>
    public sealed class CoreApi
    {
        private readonly ICore core_;

        internal CoreApi(ICore core)
        {
            core_ = core;
        }

        /// <summary>Run a check (any module's) and return its result.</summary>
        public NscpResult SimpleQuery(string command, params string[] arguments)
        {
            var message = new PB.Commands.QueryRequestMessage();
            var request = new PB.Commands.QueryRequestMessage.Types.Request { Command = command ?? string.Empty };
            if (arguments != null) request.Arguments.AddRange(arguments);
            message.Payload.Add(request);
            var result = core_.query(message.ToByteArray());
            if (!result.result) return new NscpResult(3, "Failed to run: " + command, string.Empty);
            var reply = PB.Commands.QueryResponseMessage.Parser.ParseFrom(result.data);
            if (reply.Payload.Count == 0) return new NscpResult(3, "No response from: " + command, string.Empty);
            var payload = reply.Payload[0];
            var text = new List<string>();
            var perf = new List<string>();
            foreach (var line in payload.Lines)
            {
                text.Add(line.Message ?? string.Empty);
                var rendered = PerfData.Format(line);
                if (rendered.Length > 0) perf.Add(rendered);
            }
            return new NscpResult((int)payload.Result, string.Join("\n", text), string.Join(" ", perf));
        }

        /// <summary>Run a command-line style command and return its output lines.</summary>
        public string[] SimpleExec(string target, string command, params string[] arguments)
        {
            var message = new PB.Commands.ExecuteRequestMessage();
            var request = new PB.Commands.ExecuteRequestMessage.Types.Request { Command = command ?? string.Empty };
            if (arguments != null) request.Arguments.AddRange(arguments);
            message.Payload.Add(request);
            var result = core_.exec(target ?? string.Empty, message.ToByteArray());
            if (!result.result) return new[] { "Failed to run: " + command };
            var reply = PB.Commands.ExecuteResponseMessage.Parser.ParseFrom(result.data);
            var lines = new List<string>();
            foreach (var payload in reply.Payload) lines.Add(payload.Message ?? string.Empty);
            return lines.ToArray();
        }

        /// <summary>Submit a passive result to a channel.</summary>
        public bool SimpleSubmit(string channel, string command, object code, string message, string perf)
        {
            var request = new PB.Commands.SubmitRequestMessage { Channel = channel ?? string.Empty };
            var payload = new PB.Commands.QueryResponseMessage.Types.Response
            {
                Command = command ?? string.Empty,
                Result = NagiosStatus.Code(NagiosStatus.Parse(code, 3)),
            };
            var line = new PB.Commands.QueryResponseMessage.Types.Response.Types.Line { Message = message ?? string.Empty };
            PerfData.Parse(perf, line);
            payload.Lines.Add(line);
            request.Payload.Add(payload);
            var result = core_.submit(channel ?? string.Empty, request.ToByteArray());
            return result.result;
        }

        /// <summary>Ask the agent to reload a module (empty for the whole agent).</summary>
        public bool Reload(string module)
        {
            return core_.reload(module ?? string.Empty);
        }

        /// <summary>Expand the NSClient++ path variables in a path.</summary>
        public string ExpandPath(string path)
        {
            return core_.expandPath(path);
        }
    }

    /// <summary>
    /// What this script answers: checks, command-line commands and channels.
    /// A handler is the name of a function in the script, or a script block.
    /// </summary>
    public sealed class RegistryApi
    {
        private readonly ScriptManager manager_;
        private readonly string script_;

        internal RegistryApi(ScriptManager manager, string script)
        {
            manager_ = manager;
            script_ = script;
        }

        /// <summary>
        /// Answer the check <paramref name="command"/> with
        /// <paramref name="handler"/>, which is called as
        /// <c>handler &lt;command&gt; &lt;arguments&gt;</c> and returns the
        /// status, message and (optionally) performance data.
        /// </summary>
        public bool SimpleQuery(string command, string description, object handler)
        {
            return manager_.RegisterQuery(script_, command, description, handler);
        }

        /// <summary>Answer the command-line command <paramref name="command"/>.</summary>
        public bool SimpleCmdline(string command, object handler)
        {
            return manager_.RegisterCommand(script_, command, handler);
        }

        /// <summary>
        /// Receive the passive results submitted to <paramref name="channel"/>.
        /// The handler is called as
        /// <c>handler &lt;channel&gt; &lt;command&gt; &lt;status&gt; &lt;message&gt; &lt;perf&gt;</c>.
        /// </summary>
        public bool SimpleSubscription(string channel, object handler)
        {
            return manager_.RegisterChannel(script_, channel, handler);
        }
    }

    /// <summary>The agent's settings, as a script sees them.</summary>
    public sealed class SettingsApi
    {
        private readonly SettingsHelper settings_;
        private readonly ICore core_;

        internal SettingsApi(ICore core, int pluginId)
        {
            core_ = core;
            settings_ = new SettingsHelper(core, pluginId);
        }

        /// <summary>The keys under a settings path.</summary>
        public string[] GetSection(string path)
        {
            return settings_.getKeys(path).ToArray();
        }

        /// <summary>The sections below a settings path, relative to it.</summary>
        public string[] GetSections(string path)
        {
            return settings_.getSections(path).ToArray();
        }

        public string GetString(string path, string key, string defaultValue)
        {
            return settings_.getString(path, key, defaultValue);
        }

        public void SetString(string path, string key, string value)
        {
            settings_.setString(path, key, value);
        }

        public bool GetBool(string path, string key, bool defaultValue)
        {
            var value = (settings_.getString(path, key, defaultValue ? "true" : "false") ?? string.Empty).Trim().ToLowerInvariant();
            if (value.Length == 0) return defaultValue;
            switch (value)
            {
                case "true":
                case "1":
                case "yes":
                case "on":
                case "enabled":
                    return true;
                case "false":
                case "0":
                case "no":
                case "off":
                case "disabled":
                    return false;
                default:
                    return defaultValue;
            }
        }

        public void SetBool(string path, string key, bool value)
        {
            settings_.setString(path, key, value ? "true" : "false");
        }

        public long GetInt(string path, string key, long defaultValue)
        {
            var value = settings_.getString(path, key, defaultValue.ToString(System.Globalization.CultureInfo.InvariantCulture));
            return long.TryParse(value, out var number) ? number : defaultValue;
        }

        public void SetInt(string path, string key, long value)
        {
            settings_.setString(path, key, value.ToString(System.Globalization.CultureInfo.InvariantCulture));
        }

        /// <summary>Describe a settings path, so it shows up in the settings UI and the reference.</summary>
        public bool RegisterPath(string path, string title, string description, bool advanced)
        {
            return settings_.registerPath(path, title, description, advanced);
        }

        /// <summary>Describe a settings key.</summary>
        public bool RegisterKey(string path, string key, string title, string description, string defaultValue, bool advanced)
        {
            return settings_.registerKey(path, key, 0, title, description, defaultValue, advanced);
        }

        /// <summary>Write the settings back to their store.</summary>
        public bool Save()
        {
            return settings_.save(string.Empty);
        }

        /// <summary>Expand the NSClient++ path variables in a path.</summary>
        public string ExpandPath(string path)
        {
            return core_.expandPath(path);
        }
    }
}
