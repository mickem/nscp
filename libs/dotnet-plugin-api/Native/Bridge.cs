// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
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
    /// <see cref="UnmanagedCallersOnlyAttribute"/> with a C (cdecl) ABI; strings
    /// are NUL-terminated UTF-8 and responses travel through a write callback.
    /// The convention is spelled out because on 32-bit Windows the default for
    /// unmanaged function pointers is stdcall, which does not match the C++
    /// side and crashes the process on the first call.
    /// Must be kept in sync with modules/DotnetPlugins/dotnet_bridge.hpp.
    /// </summary>
    public static unsafe class Bridge
    {
        /// <summary>Return codes of the routing entry points (Query/Submit/Exec/Message).</summary>
        private const int Handled = 1;
        private const int Ignored = 0;
        private const int Failed = -1;

        private sealed class LoadedPlugin
        {
            internal NativeCore Core;
            internal IPlugin Plugin;
            internal LogHelper Log;
            internal PluginLoadContext Context;
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
                context.Resolving += (ctx, name) => PluginLoadContext.ProbeDirectory(ctx, here, name);
            }
        }

        private static string Utf8(byte* s)
        {
            return s == null ? string.Empty : Marshal.PtrToStringUTF8((IntPtr)s) ?? string.Empty;
        }

        private static byte[] Bytes(byte* data, int length)
        {
            return length > 0 && data != null ? new ReadOnlySpan<byte>(data, length).ToArray() : Array.Empty<byte>();
        }

        private static void Write(delegate* unmanaged[Cdecl]<void*, byte*, int, void> write, void* wctx, byte[] data)
        {
            if (write == null || data == null || data.Length == 0) return;
            fixed (byte* p = data)
            {
                write(wctx, p, data.Length);
            }
        }

        private static LoadedPlugin Find(IntPtr handle)
        {
            lock (lock_)
            {
                return plugins_.TryGetValue((long)handle, out var p) ? p : null;
            }
        }

        /// <summary>
        /// Run <paramref name="body"/> for the plugin behind <paramref name="handle"/>,
        /// turning an unknown handle into <paramref name="unknown"/> and an exception
        /// into <see cref="Failed"/> after logging it through the core (unless
        /// <paramref name="quiet"/>: the log handler must never log).
        /// </summary>
        private static int Guarded(IntPtr handle, string what, int unknown, bool quiet, Func<LoadedPlugin, int> body)
        {
            var p = Find(handle);
            if (p == null) return unknown;
            try
            {
                return body(p);
            }
            catch (Exception e)
            {
                if (!quiet) p.Log.error(what + " failed in " + p.Path + ": " + e);
                return Failed;
            }
        }

        /// <summary>
        /// Route a result: write its bytes and map success to <see cref="Handled"/>.
        /// </summary>
        private static int Deliver(Result result, delegate* unmanaged[Cdecl]<void*, byte*, int, void> write, void* wctx)
        {
            if (result == null) return Failed;
            Write(write, wctx, result.data);
            return result.result ? Handled : Failed;
        }

        /// <summary>
        /// Load a plugin assembly, instantiate its factory and create the plugin.
        /// </summary>
        /// <returns>An opaque handle (never 0) or 0 on failure (details are logged through the core).</returns>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        public static IntPtr Load(IntPtr coreCallback, IntPtr coreContext, byte* assemblyPath, byte* factoryType, byte* alias, int pluginId)
        {
            var path = Utf8(assemblyPath);
            var type = Utf8(factoryType);
            var instance = new PluginInstance(pluginId, Utf8(alias));
            var core = new NativeCore(coreCallback, coreContext, instance);
            var log = new LogHelper(core);
            PluginLoadContext context = null;
            try
            {
                if (!File.Exists(path))
                {
                    log.error("Plugin assembly not found: " + path);
                    return IntPtr.Zero;
                }
                context = new PluginLoadContext(path);
                var assembly = context.LoadFromAssemblyPath(path);
                var factoryClass = assembly.GetType(type, throwOnError: false);
                if (factoryClass == null)
                {
                    log.error("Factory class " + type + " not found in " + path);
                    context.Unload();
                    return IntPtr.Zero;
                }
                if (!(Activator.CreateInstance(factoryClass) is IPluginFactory factory))
                {
                    log.error(type + " in " + path + " does not implement NSCP.Core.IPluginFactory");
                    context.Unload();
                    return IntPtr.Zero;
                }
                var plugin = factory.create(core, instance);
                if (plugin == null)
                {
                    log.error("Factory " + type + " in " + path + " returned no plugin");
                    context.Unload();
                    return IntPtr.Zero;
                }
                var loaded = new LoadedPlugin { Core = core, Plugin = plugin, Log = log, Context = context, Path = path };
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
                try
                {
                    context?.Unload();
                }
                catch (Exception)
                {
                }
                return IntPtr.Zero;
            }
        }

        /// <summary>Call IPlugin.load(mode). Returns 1 on success, 0 on failure.</summary>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        public static int Start(IntPtr handle, int mode)
        {
            var rc = Guarded(handle, "Start", 0, false, p => p.Plugin.load(mode) ? 1 : 0);
            return rc == Failed ? 0 : rc;
        }

        /// <summary>
        /// Call IPlugin.unload(), forget the plugin, cut its link to the native
        /// module and let its load context be collected. Returns 1 on success.
        /// </summary>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        public static int Unload(IntPtr handle)
        {
            LoadedPlugin p;
            lock (lock_)
            {
                if (!plugins_.TryGetValue((long)handle, out p)) return 0;
                plugins_.Remove((long)handle);
            }
            var ok = 0;
            try
            {
                ok = p.Plugin.unload() ? 1 : 0;
            }
            catch (Exception e)
            {
                p.Log.error("Failed to unload " + p.Path + ": " + e);
            }
            // Nothing may reach the native module after this returns: the
            // module's memory goes away with it.
            p.Core.Detach();
            try
            {
                p.Context?.Unload();
            }
            catch (Exception)
            {
                // Best effort; the context is collected when the plugin lets go of its objects.
            }
            return ok;
        }

        /// <summary>
        /// Write "name\nversion\ndescription" for the plugin (for log lines).
        /// </summary>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        public static int Describe(IntPtr handle, delegate* unmanaged[Cdecl]<void*, byte*, int, void> write, void* wctx)
        {
            var rc = Guarded(handle, "Describe", 0, false, p =>
            {
                var text = (p.Plugin.getName() ?? "") + "\n" + (p.Plugin.getVersion()?.ToString() ?? "") + "\n" + (p.Plugin.getDescription() ?? "");
                Write(write, wctx, Encoding.UTF8.GetBytes(text));
                return 1;
            });
            return rc == Failed ? 0 : rc;
        }

        /// <summary>
        /// Route a query to the plugin if it registered the command.
        /// </summary>
        /// <returns>1 handled (response written), 0 not this plugin's command, -1 failed.</returns>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        public static int Query(IntPtr handle, byte* command, byte* request, int requestLength, delegate* unmanaged[Cdecl]<void*, byte*, int, void> write, void* wctx)
        {
            var cmd = Utf8(command);
            return Guarded(handle, "Command " + cmd, Ignored, false, p =>
            {
                if (!p.Core.HandlesQuery(cmd)) return Ignored;
                var handler = p.Plugin.getQueryHandler();
                if (handler == null || !handler.isActive()) return Ignored;
                return Deliver(handler.onQuery(cmd, Bytes(request, requestLength)), write, wctx);
            });
        }

        /// <summary>
        /// Route a passive result (a serialized SubmitRequestMessage) to the
        /// plugin if it registered the channel.
        /// </summary>
        /// <returns>1 handled (SubmitResponseMessage written), 0 not this plugin's channel, -1 failed.</returns>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        public static int Submit(IntPtr handle, byte* channel, byte* request, int requestLength, delegate* unmanaged[Cdecl]<void*, byte*, int, void> write, void* wctx)
        {
            var chan = Utf8(channel);
            return Guarded(handle, "Submission on " + chan, Ignored, false, p =>
            {
                if (!p.Core.HandlesChannel(chan)) return Ignored;
                var handler = p.Plugin.getSubmissionHandler();
                if (handler == null || !handler.isActive()) return Ignored;
                return Deliver(handler.onSubmission(chan, Bytes(request, requestLength)), write, wctx);
            });
        }

        /// <summary>
        /// Route a command-line style command (a serialized
        /// ExecuteRequestMessage) to the plugin if it registered the command.
        /// </summary>
        /// <returns>1 handled (ExecuteResponseMessage written), 0 not this plugin's command, -1 failed.</returns>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        public static int Exec(IntPtr handle, byte* target, byte* command, byte* request, int requestLength, delegate* unmanaged[Cdecl]<void*, byte*, int, void> write, void* wctx)
        {
            var cmd = Utf8(command);
            var tgt = Utf8(target);
            return Guarded(handle, "Command " + cmd, Ignored, false, p =>
            {
                if (!p.Core.HandlesCommand(cmd)) return Ignored;
                var handler = p.Plugin.getExecutionHandler();
                if (handler == null || !handler.isActive()) return Ignored;
                return Deliver(handler.onCommand(tgt, cmd, Bytes(request, requestLength)), write, wctx);
            });
        }

        /// <summary>
        /// 1 when the plugin exposes an <see cref="IMessageHandler"/>, 0 otherwise.
        /// The native module asks once after Start and only hands log entries
        /// to plugins that answered 1, so the others cost nothing per log line.
        /// </summary>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        public static int HasMessageHandler(IntPtr handle)
        {
            var rc = Guarded(handle, "HasMessageHandler", 0, false, p => p.Plugin.getMessageHandler() != null ? 1 : 0);
            return rc == Failed ? 0 : rc;
        }

        /// <summary>
        /// Hand a log entry (a serialized LogEntry) to the plugin's message
        /// handler. Never logs itself: that would feed straight back in here.
        /// </summary>
        /// <returns>1 handled, 0 the plugin has no active message handler, -1 the handler threw.</returns>
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        public static int Message(IntPtr handle, byte* request, int requestLength)
        {
            return Guarded(handle, "Message", Ignored, true, p =>
            {
                var handler = p.Plugin.getMessageHandler();
                if (handler == null || !handler.isActive()) return Ignored;
                return handler.onMessage(Bytes(request, requestLength)) ? Handled : Failed;
            });
        }
    }
}
