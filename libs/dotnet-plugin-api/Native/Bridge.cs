// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Text;
using NSCP.Core;
using NSCP.Helpers;

namespace NSCP.Core.Native
{
    /// <summary>
    /// Entry points the C++ DotnetPlugins module resolves through
    /// <c>load_assembly_and_get_function_pointer</c>. All of them are
    /// <see cref="UnmanagedCallersOnlyAttribute"/> with a C ABI; strings are
    /// NUL-terminated UTF-8 and responses travel through a write callback.
    /// Must be kept in sync with modules/DotnetPlugins/dotnet_bridge.hpp.
    /// </summary>
    public static unsafe class Bridge
    {
        /// <summary>Return codes of <see cref="Query"/>.</summary>
        private const int QueryHandled = 1;
        private const int QueryIgnored = 0;
        private const int QueryFailed = -1;

        private sealed class LoadedPlugin
        {
            internal NativeCore Core;
            internal IPlugin Plugin;
            internal LogHelper Log;
            internal string Path;
        }

        private static readonly object lock_ = new object();
        private static readonly Dictionary<long, LoadedPlugin> plugins_ = new Dictionary<long, LoadedPlugin>();
        private static long nextHandle_ = 1;

        static Bridge()
        {
            // hostfxr loads this assembly into its own isolated context and
            // resolves its dependencies (Google.Protobuf) through
            // NSCP.Core.deps.json. Should that fail (a hand-copied install with
            // no deps.json), still find whatever is shipped next to us.
            var here = System.IO.Path.GetDirectoryName(typeof(Bridge).Assembly.Location);
            var context = AssemblyLoadContext.GetLoadContext(typeof(Bridge).Assembly);
            if (!string.IsNullOrEmpty(here) && context != null)
            {
                context.Resolving += (ctx, name) =>
                {
                    var candidate = System.IO.Path.Combine(here, name.Name + ".dll");
                    return File.Exists(candidate) ? ctx.LoadFromAssemblyPath(candidate) : null;
                };
            }
        }

        private static string Utf8(byte* s)
        {
            return s == null ? string.Empty : Marshal.PtrToStringUTF8((IntPtr)s) ?? string.Empty;
        }

        private static void Write(delegate* unmanaged<void*, byte*, int, void> write, void* wctx, byte[] data)
        {
            if (write == null || data == null || data.Length == 0) return;
            fixed (byte* p = data)
            {
                write(wctx, p, data.Length);
            }
        }

        private static void WriteString(delegate* unmanaged<void*, byte*, int, void> write, void* wctx, string text)
        {
            Write(write, wctx, Encoding.UTF8.GetBytes(text ?? string.Empty));
        }

        private static LoadedPlugin Find(IntPtr handle)
        {
            lock (lock_)
            {
                return plugins_.TryGetValue((long)handle, out var p) ? p : null;
            }
        }

        /// <summary>
        /// Load a plugin assembly, instantiate its factory and create the plugin.
        /// </summary>
        /// <returns>An opaque handle (never 0) or 0 on failure (details are logged through the core).</returns>
        [UnmanagedCallersOnly]
        public static IntPtr Load(IntPtr coreCallback, IntPtr coreContext, byte* assemblyPath, byte* factoryType, byte* alias, int pluginId)
        {
            var path = Utf8(assemblyPath);
            var type = Utf8(factoryType);
            var instance = new PluginInstance(pluginId, Utf8(alias));
            var core = new NativeCore(coreCallback, coreContext, instance);
            var log = new LogHelper(core);
            try
            {
                if (!File.Exists(path))
                {
                    log.error("Plugin assembly not found: " + path);
                    return IntPtr.Zero;
                }
                var context = new PluginLoadContext(path);
                var assembly = context.LoadFromAssemblyPath(path);
                var factoryClass = assembly.GetType(type, throwOnError: false);
                if (factoryClass == null)
                {
                    log.error("Factory class " + type + " not found in " + path);
                    return IntPtr.Zero;
                }
                if (!(Activator.CreateInstance(factoryClass) is IPluginFactory factory))
                {
                    log.error(type + " in " + path + " does not implement NSCP.Core.IPluginFactory");
                    return IntPtr.Zero;
                }
                var plugin = factory.create(core, instance);
                if (plugin == null)
                {
                    log.error("Factory " + type + " in " + path + " returned no plugin");
                    return IntPtr.Zero;
                }
                var loaded = new LoadedPlugin { Core = core, Plugin = plugin, Log = log, Path = path };
                lock (lock_)
                {
                    var handle = nextHandle_++;
                    plugins_[handle] = loaded;
                    return (IntPtr)handle;
                }
            }
            catch (Exception e)
            {
                log.error("Failed to load " + path + " (" + type + "): " + e);
                return IntPtr.Zero;
            }
        }

        /// <summary>Call IPlugin.load(mode). Returns 1 on success, 0 on failure.</summary>
        [UnmanagedCallersOnly]
        public static int Start(IntPtr handle, int mode)
        {
            var p = Find(handle);
            if (p == null) return 0;
            try
            {
                return p.Plugin.load(mode) ? 1 : 0;
            }
            catch (Exception e)
            {
                p.Log.error("Failed to start " + p.Path + ": " + e);
                return 0;
            }
        }

        /// <summary>Call IPlugin.unload() and forget the plugin. Returns 1 on success.</summary>
        [UnmanagedCallersOnly]
        public static int Unload(IntPtr handle)
        {
            LoadedPlugin p;
            lock (lock_)
            {
                if (!plugins_.TryGetValue((long)handle, out p)) return 0;
                plugins_.Remove((long)handle);
            }
            try
            {
                return p.Plugin.unload() ? 1 : 0;
            }
            catch (Exception e)
            {
                p.Log.error("Failed to unload " + p.Path + ": " + e);
                return 0;
            }
        }

        /// <summary>
        /// Write "name\nversion\ndescription" for the plugin (for log lines).
        /// </summary>
        [UnmanagedCallersOnly]
        public static int Describe(IntPtr handle, delegate* unmanaged<void*, byte*, int, void> write, void* wctx)
        {
            var p = Find(handle);
            if (p == null) return 0;
            try
            {
                WriteString(write, wctx, (p.Plugin.getName() ?? "") + "\n" + (p.Plugin.getVersion()?.ToString() ?? "") + "\n" + (p.Plugin.getDescription() ?? ""));
                return 1;
            }
            catch (Exception e)
            {
                p.Log.error("Failed to describe " + p.Path + ": " + e);
                return 0;
            }
        }

        /// <summary>
        /// Route a query to the plugin if it registered the command.
        /// </summary>
        /// <returns>1 handled (response written), 0 not this plugin's command, -1 failed.</returns>
        [UnmanagedCallersOnly]
        public static int Query(IntPtr handle, byte* command, byte* request, int requestLength, delegate* unmanaged<void*, byte*, int, void> write, void* wctx)
        {
            var p = Find(handle);
            if (p == null) return QueryIgnored;
            var cmd = Utf8(command);
            try
            {
                if (!p.Core.HandlesCommand(cmd)) return QueryIgnored;
                var handler = p.Plugin.getQueryHandler();
                if (handler == null || !handler.isActive()) return QueryIgnored;
                var payload = requestLength > 0 && request != null ? new ReadOnlySpan<byte>(request, requestLength).ToArray() : Array.Empty<byte>();
                var result = handler.onQuery(cmd, payload);
                if (result == null) return QueryFailed;
                Write(write, wctx, result.data);
                return result.result ? QueryHandled : QueryFailed;
            }
            catch (Exception e)
            {
                p.Log.error("Command " + cmd + " failed in " + p.Path + ": " + e);
                return QueryFailed;
            }
        }
    }
}
