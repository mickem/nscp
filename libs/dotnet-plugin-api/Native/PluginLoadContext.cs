// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

using System;
using System.IO;
using System.Reflection;
using System.Runtime.Loader;

namespace NSCP.Core.Native
{
    /// <summary>
    /// Isolates each plugin assembly (and its private dependencies, resolved
    /// through its <c>.deps.json</c>) while sharing the contract assemblies
    /// with the host so the <see cref="IPlugin"/> types are the same on both
    /// sides of the boundary.
    /// </summary>
    internal sealed class PluginLoadContext : AssemblyLoadContext
    {
        private readonly AssemblyDependencyResolver resolver_;
        private readonly string directory_;

        internal PluginLoadContext(string pluginPath)
            : base(Path.GetFileNameWithoutExtension(pluginPath), isCollectible: false)
        {
            resolver_ = new AssemblyDependencyResolver(pluginPath);
            directory_ = Path.GetDirectoryName(pluginPath) ?? string.Empty;
        }

        /// <summary>
        /// The contract assemblies must be the very instances the host loaded
        /// (hostfxr puts NSCP.Core in its own isolated context, not the default
        /// one), otherwise the plugin's IPluginFactory is a different type than
        /// the one the bridge casts to.
        /// </summary>
        private static Assembly SharedAssembly(AssemblyName name)
        {
            var host = typeof(PluginLoadContext).Assembly;
            if (string.Equals(name.Name, host.GetName().Name, StringComparison.OrdinalIgnoreCase)) return host;
            var protobuf = typeof(Google.Protobuf.IMessage).Assembly;
            if (string.Equals(name.Name, protobuf.GetName().Name, StringComparison.OrdinalIgnoreCase)) return protobuf;
            var hostContext = GetLoadContext(host);
            if (hostContext != null)
            {
                foreach (var loaded in hostContext.Assemblies)
                {
                    if (string.Equals(loaded.GetName().Name, name.Name, StringComparison.OrdinalIgnoreCase)) return loaded;
                }
            }
            return null;
        }

        protected override Assembly Load(AssemblyName name)
        {
            var shared = SharedAssembly(name);
            if (shared != null) return shared;
            var path = resolver_.ResolveAssemblyToPath(name);
            if (path == null)
            {
                var candidate = Path.Combine(directory_, name.Name + ".dll");
                if (File.Exists(candidate)) path = candidate;
            }
            // null => fall back to the default context (the framework).
            return path != null ? LoadFromAssemblyPath(path) : null;
        }

        protected override IntPtr LoadUnmanagedDll(string unmanagedDllName)
        {
            var path = resolver_.ResolveUnmanagedDllToPath(unmanagedDllName);
            return path != null ? LoadUnmanagedDllFromPath(path) : IntPtr.Zero;
        }
    }
}
