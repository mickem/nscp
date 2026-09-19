// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Management.Automation;
using System.Management.Automation.Runspaces;
using System.Runtime.InteropServices;
using System.Text;

namespace NSCP.PowerShell
{
    /// <summary>What running a handler produced.</summary>
    internal sealed class ScriptOutput
    {
        internal Collection<PSObject> Objects;
        internal readonly List<string> Information = new List<string>();
        internal readonly List<string> Errors = new List<string>();

        internal bool Failed
        {
            get { return Errors.Count > 0; }
        }

        internal string ErrorText
        {
            get { return string.Join("; ", Errors); }
        }
    }

    /// <summary>
    /// One script, and the runspace it lives in.
    ///
    /// A script keeps its runspace for the life of the module, the way a Lua
    /// script keeps its lua_State: the functions, variables and modules it set
    /// up while loading are what its check handlers run against. The runspace
    /// is not thread safe, so every call into it is serialised - two checks
    /// answered by the same script run one after the other, as they do in Lua.
    /// </summary>
    internal sealed class ScriptRunspace : IDisposable
    {
        private readonly object lock_ = new object();
        private Runspace runspace_;
        // True while a pipeline is running in this runspace. Only ever seen by
        // the thread that is running it: another thread is still waiting for
        // lock_ and cannot get this far. See Run().
        private bool running_;

        internal ScriptRunspace(string alias, string path)
        {
            Alias = alias;
            Path = path;
        }

        /// <summary>The alias the script is configured under.</summary>
        internal string Alias { get; }

        /// <summary>The full path of the script file.</summary>
        internal string Path { get; }

        /// <summary>
        /// Create the runspace, publish <c>$nscp</c> in it and run the script,
        /// which is where it registers what it answers. Dot-sourced, so the
        /// functions it defines stay callable afterwards.
        /// </summary>
        internal ScriptOutput Open(NscpApi api, string workingDirectory, string executionPolicy)
        {
            lock (lock_)
            {
                var state = InitialSessionState.CreateDefault2();
                // Run pipelines on the thread that calls Invoke rather than on
                // a thread of the runspace's own. Access here is serialised
                // anyway, and it is what makes a check that queries another
                // script work: that query comes back into the module on this
                // same thread, so the locks it meets on the way are the ones it
                // already holds.
                state.ThreadOptions = PSThreadOptions.UseCurrentThread;
                state.Variables.Add(new SessionStateVariableEntry("nscp", api, "The NSClient++ API"));
                ApplyExecutionPolicy(state, executionPolicy);
                runspace_ = RunspaceFactory.CreateRunspace(state);
                runspace_.Open();
                var script = new StringBuilder();
                if (!string.IsNullOrEmpty(workingDirectory) && Directory.Exists(workingDirectory))
                {
                    script.Append("Set-Location -LiteralPath ").Append(Quote(workingDirectory)).Append("; ");
                }
                script.Append(". ").Append(Quote(Path));
                return Run(ps => ps.AddScript(script.ToString(), false));
            }
        }

        /// <summary>True when the script defines a function with this name.</summary>
        internal bool HasFunction(string name)
        {
            lock (lock_)
            {
                if (runspace_ == null) return false;
                var output = Run(ps => ps.AddCommand("Get-Command")
                    .AddParameter("Name", name)
                    .AddParameter("CommandType", CommandTypes.Function | CommandTypes.Filter)
                    .AddParameter("ErrorAction", "SilentlyContinue"));
                return output.Objects != null && output.Objects.Count > 0;
            }
        }

        /// <summary>
        /// Call a handler: the name of a function in the script, or a script
        /// block. Arguments are passed positionally, so a handler declares them
        /// with a plain <c>param(...)</c>.
        /// </summary>
        internal ScriptOutput Invoke(object handler, object[] arguments)
        {
            var target = Unwrap(handler);
            lock (lock_)
            {
                if (runspace_ == null)
                {
                    var closed = new ScriptOutput();
                    closed.Errors.Add("The script is not loaded: " + Path);
                    return closed;
                }
                if (target is ScriptBlock block)
                {
                    // Invoke-Command runs the block in this runspace, which is
                    // the one it was created in, so its closure still works.
                    return Run(ps => ps.AddCommand("Invoke-Command")
                        .AddParameter("ScriptBlock", block)
                        .AddParameter("ArgumentList", arguments ?? Array.Empty<object>()));
                }
                var name = target as string ?? target?.ToString();
                if (string.IsNullOrWhiteSpace(name))
                {
                    var missing = new ScriptOutput();
                    missing.Errors.Add("No handler to call in " + Path);
                    return missing;
                }
                return Run(ps =>
                {
                    ps.AddCommand(name);
                    foreach (var argument in arguments ?? Array.Empty<object>()) ps.AddArgument(argument);
                    return ps;
                });
            }
        }

        public void Dispose()
        {
            lock (lock_)
            {
                var runspace = runspace_;
                runspace_ = null;
                try
                {
                    runspace?.Close();
                    runspace?.Dispose();
                }
                catch (Exception)
                {
                    // Nothing useful to do while tearing down.
                }
            }
        }

        /// <summary>
        /// A handler that came from PowerShell arrives wrapped in a PSObject.
        /// </summary>
        private static object Unwrap(object handler)
        {
            return handler is PSObject wrapped ? wrapped.BaseObject : handler;
        }

        /// <summary>Single quoted, the way PowerShell escapes a literal path.</summary>
        private static string Quote(string value)
        {
            return "'" + (value ?? string.Empty).Replace("'", "''") + "'";
        }

        /// <summary>
        /// The execution policy only exists on Windows; setting it anywhere
        /// else throws. An empty setting leaves the machine's policy alone.
        /// </summary>
        private static void ApplyExecutionPolicy(InitialSessionState state, string policy)
        {
            if (string.IsNullOrWhiteSpace(policy)) return;
            if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows)) return;
            if (Enum.TryParse(policy.Trim(), ignoreCase: true, out Microsoft.PowerShell.ExecutionPolicy parsed)) state.ExecutionPolicy = parsed;
        }

        private ScriptOutput Run(Func<System.Management.Automation.PowerShell, System.Management.Automation.PowerShell> build)
        {
            var output = new ScriptOutput();
            if (running_)
            {
                // A handler asked the agent for a check its own script answers.
                // A runspace runs one pipeline at a time, so this cannot be
                // served - say so instead of stalling the check until whatever
                // timeout is watching it fires.
                output.Errors.Add("A PowerShell script cannot run a command it answers itself (" + Path +
                                  "); move the command it needs into another script.");
                return output;
            }
            using (var ps = System.Management.Automation.PowerShell.Create())
            {
                ps.Runspace = runspace_;
                build(ps);
                try
                {
                    running_ = true;
                    output.Objects = ps.Invoke();
                }
                catch (Exception e)
                {
                    // Everything the script can do wrong ends up here, not only
                    // a PowerShell RuntimeException: a broken runspace throws
                    // PSInvalidOperationException, and a .NET type a script
                    // reached for throws whatever it throws. All of it is the
                    // script's error to report, never the module's to die of.
                    output.Errors.Add(e.Message);
                }
                finally
                {
                    running_ = false;
                }
                foreach (var record in ps.Streams.Error) output.Errors.Add(record.ToString());
                foreach (var record in ps.Streams.Information) output.Information.Add(record.ToString());
                foreach (var record in ps.Streams.Warning) output.Information.Add(record.ToString());
                ps.Streams.ClearStreams();
            }
            return output;
        }
    }

    /// <summary>
    /// Reading a check result out of whatever a handler returned.
    /// </summary>
    internal static class ScriptResult
    {
        /// <summary>
        /// The shapes a handler may return, in the order they are recognised:
        ///
        ///   @('ok', 'Everything is fine', "'load'=0.5")   status, message, perf
        ///   @{ code = 'ok'; message = '...'; perf = '' }  the same, by name
        ///   'OK: everything is fine|load=0.5'             message only, status OK
        ///
        /// Anything else is turned into text, one output object per line. A
        /// handler that wrote with Write-Host instead of returning is read from
        /// the information stream, which is what a script ported from the old
        /// `[/modules/powershell/commands]` module does.
        /// </summary>
        internal static void Read(ScriptOutput output, out int code, out string message, out string perf)
        {
            code = 0;
            message = string.Empty;
            perf = string.Empty;
            var objects = output.Objects;
            if (objects == null || objects.Count == 0)
            {
                message = string.Join("\n", output.Information);
                if (message.Length == 0) message = "No output from the script";
                return;
            }
            if (objects.Count == 1)
            {
                var value = objects[0]?.BaseObject;
                if (value is IDictionary map)
                {
                    code = NagiosStatus.Parse(Lookup(map, "code", "status", "result"), 0);
                    message = Text(Lookup(map, "message", "msg", "text"));
                    perf = Text(Lookup(map, "perf", "perfdata", "performance"));
                    return;
                }
                if (value is object[] || value is ArrayList)
                {
                    ReadList(value is object[] array ? array : ((ArrayList)value).ToArray(), ref code, ref message, ref perf);
                    return;
                }
            }
            if (objects.Count <= 3 && LooksLikeStatus(objects[0]))
            {
                var array = new object[objects.Count];
                for (var i = 0; i < objects.Count; i++) array[i] = objects[i]?.BaseObject;
                ReadList(array, ref code, ref message, ref perf);
                return;
            }
            var lines = new List<string>();
            foreach (var item in objects) lines.Add(Text(item));
            message = string.Join("\n", lines);
        }

        /// <summary>
        /// A returned list is (status, message, perf) only when its first item
        /// really is a status word or number; @('a', 'b') is two output lines.
        /// </summary>
        private static bool LooksLikeStatus(PSObject value)
        {
            var text = value?.BaseObject as string;
            if (text == null) return value?.BaseObject is int || value?.BaseObject is long;
            switch (text.Trim().ToLowerInvariant())
            {
                case "ok":
                case "warn":
                case "warning":
                case "crit":
                case "critical":
                case "unknown":
                case "0":
                case "1":
                case "2":
                case "3":
                    return true;
                default:
                    return false;
            }
        }

        private static void ReadList(object[] items, ref int code, ref string message, ref string perf)
        {
            if (items.Length > 0) code = NagiosStatus.Parse(items[0], 0);
            if (items.Length > 1) message = Text(items[1]);
            if (items.Length > 2) perf = Text(items[2]);
            if (items.Length == 1)
            {
                // A bare status and nothing else: say so rather than reporting
                // an empty check result.
                code = NagiosStatus.Parse(items[0], 0);
                message = NagiosStatus.Name(code);
            }
        }

        private static object Lookup(IDictionary map, params string[] keys)
        {
            foreach (DictionaryEntry entry in map)
            {
                var key = entry.Key?.ToString();
                if (key == null) continue;
                foreach (var candidate in keys)
                {
                    if (string.Equals(key, candidate, StringComparison.OrdinalIgnoreCase)) return entry.Value;
                }
            }
            return null;
        }

        private static string Text(object value)
        {
            if (value == null) return string.Empty;
            if (value is PSObject wrapped) return wrapped.BaseObject?.ToString() ?? string.Empty;
            return value.ToString() ?? string.Empty;
        }
    }
}
