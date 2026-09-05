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
    /// ownership.
    /// </summary>
    internal sealed unsafe class NativeCore : ICore, IPluginCore
    {
        private readonly delegate* unmanaged[Cdecl]<void*, int, byte*, byte*, int, delegate* unmanaged[Cdecl]<void*, byte*, int, void>, void*, int> core_;
        private readonly void* ctx_;
        private readonly PluginInstance instance_;
        private readonly HashSet<string> commands_ = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

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

        /// <summary>Commands the plugin registered as queries; used to route.</summary>
        internal bool HandlesCommand(string command)
        {
            lock (commands_)
            {
                return commands_.Contains(command);
            }
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
            RememberRegisteredQueries(request);
            return Call(CoreOp.Registry, null, request);
        }

        public void log(byte[] request)
        {
            Call(CoreOp.Log, null, request);
        }

        /// <summary>
        /// Peek at registry requests so query registrations can be routed back
        /// to this plugin without the native side having to parse protobuf.
        /// </summary>
        private void RememberRegisteredQueries(byte[] request)
        {
            try
            {
                var msg = PB.Registry.RegistryRequestMessage.Parser.ParseFrom(request ?? Array.Empty<byte>());
                foreach (var payload in msg.Payload)
                {
                    var reg = payload.Registration;
                    if (reg == null || reg.Type != PB.Registry.ItemType.Query || string.IsNullOrEmpty(reg.Name))
                        continue;
                    lock (commands_)
                    {
                        if (reg.Unregister)
                        {
                            commands_.Remove(reg.Name);
                            foreach (var alias in reg.Alias) commands_.Remove(alias);
                        }
                        else
                        {
                            commands_.Add(reg.Name);
                            foreach (var alias in reg.Alias) commands_.Add(alias);
                        }
                    }
                }
            }
            catch (Exception)
            {
                // Not a well-formed registry message: let the core reject it.
            }
        }

        private Result Call(CoreOp op, string str, byte[] request)
        {
            request ??= Array.Empty<byte>();
            var sink = new ResponseSink();
            var sinkHandle = GCHandle.Alloc(sink);
            byte[] strBytes = str == null ? null : NullTerminated(str);
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

        private static byte[] NullTerminated(string s)
        {
            var utf8 = System.Text.Encoding.UTF8.GetBytes(s);
            var buf = new byte[utf8.Length + 1];
            Buffer.BlockCopy(utf8, 0, buf, 0, utf8.Length);
            return buf;
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        private static void WriteResponse(void* wctx, byte* data, int len)
        {
            if (wctx == null || len <= 0 || data == null) return;
            var sink = (ResponseSink)GCHandle.FromIntPtr((IntPtr)wctx).Target;
            sink?.Append(new ReadOnlySpan<byte>(data, len));
        }

        private sealed class ResponseSink
        {
            private byte[] buffer_ = Array.Empty<byte>();

            internal void Append(ReadOnlySpan<byte> chunk)
            {
                var next = new byte[buffer_.Length + chunk.Length];
                Buffer.BlockCopy(buffer_, 0, next, 0, buffer_.Length);
                chunk.CopyTo(new Span<byte>(next, buffer_.Length, chunk.Length));
                buffer_ = next;
            }

            internal byte[] ToArray()
            {
                return buffer_;
            }
        }
    }
}
