// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using NSCP.Core;

namespace NSCP.Core.Native
{
    /// <summary>
    /// The operations the native module exposes through its single core
    /// callback. Must match <c>dotnet::core_op</c> in
    /// modules/DotnetPlugins/dotnet_bridge.hpp.
    /// </summary>
    internal enum CoreOp
    {
        Query = 1,
        Exec = 2,
        Submit = 3,
        Reload = 4,
        Settings = 5,
        Registry = 6,
        Log = 7,
    }

    /// <summary>
    /// <see cref="ICore"/> backed by the C++ module. Every call marshals the
    /// request bytes to the native side and collects the response through a
    /// write callback, so no memory crosses the boundary with ambiguous
    /// ownership. All function pointers are cdecl: on 32-bit Windows the
    /// default for unmanaged pointers is stdcall, which would not match the C++
    /// side.
    /// </summary>
    internal sealed unsafe class NativeCore : ICore, IPluginCore
    {
        private readonly delegate* unmanaged[Cdecl]<void*, int, byte*, byte*, int, delegate* unmanaged[Cdecl]<void*, byte*, int, void>, void*, int> core_;
        private readonly void* ctx_;
        private readonly PluginInstance instance_;
        private readonly object lock_ = new object();
        private readonly HashSet<string> queries_ = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        private readonly HashSet<string> channels_ = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        private readonly HashSet<string> commands_ = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        private volatile bool detached_;

        internal NativeCore(IntPtr core, IntPtr ctx, PluginInstance instance)
        {
            core_ = (delegate* unmanaged[Cdecl]<void*, int, byte*, byte*, int, delegate* unmanaged[Cdecl]<void*, byte*, int, void>, void*, int>)core;
            ctx_ = (void*)ctx;
            instance_ = instance;
        }

        public PluginInstance getInstance()
        {
            return instance_;
        }

        /// <summary>
        /// Cut the link to the native module. The runtime cannot unload, so a
        /// plugin thread that outlives its module (a timer it forgot to stop)
        /// may still call in here after the module's memory is gone; from then
        /// on every call fails instead of jumping into a freed callback.
        /// </summary>
        internal void Detach()
        {
            detached_ = true;
        }

        internal bool HandlesQuery(string command)
        {
            lock (lock_) return queries_.Contains(command);
        }

        internal bool HandlesChannel(string channel)
        {
            lock (lock_) return channels_.Contains(channel);
        }

        internal bool HandlesCommand(string command)
        {
            lock (lock_) return commands_.Contains(command);
        }

        public Result query(byte[] request)
        {
            return Call(CoreOp.Query, null, request);
        }

        public Result exec(string target, byte[] request)
        {
            return Call(CoreOp.Exec, target ?? string.Empty, request);
        }

        public Result submit(string channel, byte[] request)
        {
            return Call(CoreOp.Submit, channel ?? string.Empty, request);
        }

        public bool reload(string module)
        {
            return Call(CoreOp.Reload, module ?? string.Empty, Array.Empty<byte>()).result;
        }

        public Result settings(byte[] request)
        {
            return Call(CoreOp.Settings, null, request);
        }

        public Result registry(byte[] request)
        {
            var forward = RememberRegistrations(request);
            if (!forward)
            {
                // Command-line commands are not a registry concept in NSClient++
                // (the core routes them to the module by name and the module
                // decides); the registration is kept here for routing and
                // answered locally.
                var reply = new PB.Registry.RegistryResponseMessage();
                var payload = new PB.Registry.RegistryResponseMessage.Types.Response();
                payload.Result = new PB.Common.Result { Code = PB.Common.Result.Types.StatusCodeType.StatusOk };
                reply.Payload.Add(payload);
                return new Result(Google.Protobuf.MessageExtensions.ToByteArray(reply));
            }
            return Call(CoreOp.Registry, null, request);
        }

        public void log(byte[] request)
        {
            Call(CoreOp.Log, null, request);
        }

        /// <summary>
        /// Peek at registry requests so queries, channels and commands can be
        /// routed back to this plugin without the native side having to parse
        /// protobuf.
        /// </summary>
        /// <returns>
        /// false when the message consists only of command-line command
        /// registrations, which the core does not know about and must not see.
        /// </returns>
        private bool RememberRegistrations(byte[] request)
        {
            var forward = true;
            try
            {
                var msg = PB.Registry.RegistryRequestMessage.Parser.ParseFrom(request ?? Array.Empty<byte>());
                var onlyCommands = msg.Payload.Count > 0;
                foreach (var payload in msg.Payload)
                {
                    var reg = payload.Registration;
                    if (reg == null || string.IsNullOrEmpty(reg.Name))
                    {
                        onlyCommands = false;
                        continue;
                    }
                    HashSet<string> set;
                    switch (reg.Type)
                    {
                        case PB.Registry.ItemType.Query:
                        case PB.Registry.ItemType.QueryAlias:
                            set = queries_;
                            onlyCommands = false;
                            break;
                        case PB.Registry.ItemType.Handler:
                            set = channels_;
                            onlyCommands = false;
                            break;
                        case PB.Registry.ItemType.Command:
                            set = commands_;
                            break;
                        default:
                            onlyCommands = false;
                            continue;
                    }
                    lock (lock_)
                    {
                        if (reg.Unregister)
                        {
                            set.Remove(reg.Name);
                            foreach (var alias in reg.Alias) set.Remove(alias);
                        }
                        else
                        {
                            set.Add(reg.Name);
                            foreach (var alias in reg.Alias) set.Add(alias);
                        }
                    }
                }
                forward = !onlyCommands;
            }
            catch (Exception)
            {
                // Not a well-formed registry message: let the core reject it.
            }
            return forward;
        }

        private Result Call(CoreOp op, string str, byte[] request)
        {
            if (detached_) return new Result(false, Array.Empty<byte>());
            request ??= Array.Empty<byte>();
            var sink = new System.IO.MemoryStream();
            var sinkHandle = GCHandle.Alloc(sink);
            byte[] strBytes = str == null ? null : System.Text.Encoding.UTF8.GetBytes(str + "\0");
            int rc;
            try
            {
                fixed (byte* req = request)
                fixed (byte* s = strBytes)
                {
                    // An empty array pins to null; the native side treats (null, 0) as empty.
                    rc = core_(ctx_, (int)op, s, req, request.Length, &WriteResponse, (void*)GCHandle.ToIntPtr(sinkHandle));
                }
            }
            finally
            {
                sinkHandle.Free();
            }
            return new Result(rc != 0, sink.ToArray());
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        private static void WriteResponse(void* wctx, byte* data, int len)
        {
            if (wctx == null || len <= 0 || data == null) return;
            var sink = (System.IO.MemoryStream)GCHandle.FromIntPtr((IntPtr)wctx).Target;
            sink?.Write(new ReadOnlySpan<byte>(data, len));
        }
    }
}
