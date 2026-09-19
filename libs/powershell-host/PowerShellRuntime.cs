// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace NSCP.PowerShell
{
    /// <summary>
    /// Finds an installed PowerShell 7 and makes its assemblies loadable.
    ///
    /// NSClient++ does not ship the PowerShell engine: it is ~150 MB of
    /// assemblies, several times the size of the whole agent. This plugin is
    /// compiled against the System.Management.Automation reference assembly and
    /// loads the real engine out of $PSHOME at run time, which is exactly what
    /// a PowerShell binary module does. Windows PowerShell 5.1 is built on the
    /// .NET Framework and cannot be loaded into a .NET 8 process at all, so
    /// PowerShell 7 is the floor.
    ///
    /// <see cref="Configure"/> must be called before anything touches a type
    /// from the engine; after that the resolver answers on demand, so nothing
    /// is loaded on an agent whose scripts never run.
    /// </summary>
    internal static class PowerShellRuntime
    {
        /// <summary>The assembly whose P/Invokes need the engine's native library.</summary>
        private const string EngineAssembly = "System.Management.Automation";

        private static readonly object lock_ = new object();
        private static string configured_;
        private static string home_;
        private static bool resolved_;
        private static bool installed_;
        private static Action<string> log_ = delegate { };
        private static readonly List<string> searched_ = new List<string>();

        /// <summary>
        /// Point the resolver at a PowerShell installation and hook assembly
        /// resolution. <paramref name="home"/> is the configured
        /// 'powershell path' (empty to search); <paramref name="log"/> takes
        /// the debug lines about what was searched.
        /// </summary>
        internal static void Configure(string home, Action<string> log)
        {
            lock (lock_)
            {
                log_ = log ?? (delegate { });
                if (!string.Equals(configured_, home, StringComparison.Ordinal))
                {
                    configured_ = home;
                    // A reload may point somewhere else; look again. Assemblies
                    // already loaded stay loaded - the runtime cannot unload
                    // them - so this only affects what is resolved from now on.
                    resolved_ = false;
                    home_ = null;
                }
                if (installed_) return;
                var context = AssemblyLoadContext.GetLoadContext(typeof(PowerShellRuntime).Assembly);
                if (context == null) return;
                context.Resolving += OnResolving;
                installed_ = true;
            }
        }

        /// <summary>
        /// The PowerShell installation folder in use, or an empty string when
        /// none was found. Resolved on first use and then remembered.
        /// </summary>
        internal static string Home
        {
            get
            {
                lock (lock_)
                {
                    if (resolved_) return home_ ?? string.Empty;
                    resolved_ = true;
                    searched_.Clear();
                    foreach (var candidate in Candidates(configured_))
                    {
                        if (string.IsNullOrEmpty(candidate)) continue;
                        searched_.Add(candidate);
                        if (File.Exists(Path.Combine(candidate, EngineAssembly + ".dll")))
                        {
                            home_ = candidate;
                            log_("Using the PowerShell engine in " + candidate);
                            return home_;
                        }
                    }
                    home_ = string.Empty;
                    return home_;
                }
            }
        }

        /// <summary>Everywhere <see cref="Home"/> looked, for the error message.</summary>
        internal static string Searched
        {
            get
            {
                lock (lock_) return string.Join(", ", searched_);
            }
        }

        /// <summary>
        /// Where a PowerShell 7 install may live, most specific first: the
        /// configured folder, the environment, the pwsh on PATH and last the
        /// platform's default install folders (newest first).
        /// </summary>
        internal static IEnumerable<string> Candidates(string configured)
        {
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var candidate in RawCandidates(configured))
            {
                if (string.IsNullOrEmpty(candidate)) continue;
                var full = Full(candidate);
                if (full != null && seen.Add(full)) yield return full;
            }
        }

        private static string Full(string path)
        {
            try
            {
                return Path.GetFullPath(path.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
            }
            catch (Exception)
            {
                return null;
            }
        }

        private static IEnumerable<string> RawCandidates(string configured)
        {
            if (!string.IsNullOrEmpty(configured)) yield return configured;
            yield return Environment.GetEnvironmentVariable("NSCP_POWERSHELL_HOME");
            yield return Environment.GetEnvironmentVariable("PSHOME");
            var onPath = FindOnPath();
            if (onPath != null) yield return onPath;
            foreach (var candidate in PlatformDefaults()) yield return candidate;
        }

        /// <summary>
        /// The folder of the pwsh on PATH. Link targets are followed: the
        /// /usr/bin/pwsh most packages install is a symlink into the real
        /// install, and its own folder holds no assemblies.
        /// </summary>
        private static string FindOnPath()
        {
            var executable = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "pwsh.exe" : "pwsh";
            var path = Environment.GetEnvironmentVariable("PATH");
            if (string.IsNullOrEmpty(path)) return null;
            foreach (var directory in path.Split(Path.PathSeparator))
            {
                if (string.IsNullOrWhiteSpace(directory)) continue;
                string candidate;
                try
                {
                    candidate = Path.Combine(directory, executable);
                    if (!File.Exists(candidate)) continue;
                    var target = new FileInfo(candidate).ResolveLinkTarget(returnFinalTarget: true);
                    if (target != null) candidate = target.FullName;
                    return Path.GetDirectoryName(candidate);
                }
                catch (Exception)
                {
                    // An unreadable PATH entry is not worth failing over.
                }
            }
            return null;
        }

        private static IEnumerable<string> PlatformDefaults()
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                foreach (var variable in new[] { "ProgramFiles", "ProgramFiles(x86)", "ProgramW6432" })
                {
                    var root = Environment.GetEnvironmentVariable(variable);
                    if (string.IsNullOrEmpty(root)) continue;
                    foreach (var version in Versions(Path.Combine(root, "PowerShell"))) yield return version;
                }
                yield break;
            }
            var roots = RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? new[] { "/usr/local/microsoft/powershell", "/opt/microsoft/powershell" }
                : new[] { "/opt/microsoft/powershell", "/usr/lib/powershell", "/snap/powershell/current/opt/powershell" };
            foreach (var root in roots)
            {
                foreach (var version in Versions(root)) yield return version;
            }
        }

        /// <summary>
        /// The version folders under <paramref name="root"/>, newest first, so
        /// a machine with 7 and 7-preview side by side picks the release.
        /// </summary>
        private static IEnumerable<string> Versions(string root)
        {
            string[] directories;
            try
            {
                if (!Directory.Exists(root)) return Array.Empty<string>();
                directories = Directory.GetDirectories(root);
            }
            catch (Exception)
            {
                return Array.Empty<string>();
            }
            Array.Sort(directories, CompareVersionFolders);
            return directories;
        }

        /// <summary>
        /// Order two version folders newest first: by numeric version when both
        /// parse, and a release ahead of a prerelease of the same number
        /// ("7" before "7-preview").
        /// </summary>
        private static int CompareVersionFolders(string left, string right)
        {
            var a = Path.GetFileName(left) ?? string.Empty;
            var b = Path.GetFileName(right) ?? string.Empty;
            var dashA = a.IndexOf('-');
            var dashB = b.IndexOf('-');
            var numberA = dashA < 0 ? a : a.Substring(0, dashA);
            var numberB = dashB < 0 ? b : b.Substring(0, dashB);
            if (Version.TryParse(Pad(numberA), out var versionA) && Version.TryParse(Pad(numberB), out var versionB))
            {
                var byVersion = versionB.CompareTo(versionA);
                if (byVersion != 0) return byVersion;
            }
            if (dashA < 0 != (dashB < 0)) return dashA < 0 ? -1 : 1;
            return string.Compare(b, a, StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>System.Version needs at least two components; "7" does not parse.</summary>
        private static string Pad(string number)
        {
            return number.IndexOf('.') < 0 ? number + ".0" : number;
        }

        /// <summary>
        /// Load an assembly the plugin's own dependencies do not cover out of
        /// the PowerShell installation. The engine drags in a good part of
        /// $PSHOME (the Microsoft.PowerShell.Commands.* modules, its Json and
        /// security helpers), and every one of them lands here.
        /// </summary>
        private static Assembly OnResolving(AssemblyLoadContext context, AssemblyName name)
        {
            if (string.IsNullOrEmpty(name.Name)) return null;
            var home = Home;
            if (string.IsNullOrEmpty(home)) return null;
            var candidate = Path.Combine(home, name.Name + ".dll");
            if (!File.Exists(candidate)) return null;
            var assembly = context.LoadFromAssemblyPath(candidate);
            if (string.Equals(name.Name, EngineAssembly, StringComparison.OrdinalIgnoreCase)) InstallNativeResolver(assembly, home);
            return assembly;
        }

        /// <summary>
        /// The engine P/Invokes into a native library that ships beside it
        /// (libpsl-native on Unix). It is found through the OS search path,
        /// which does not include $PSHOME in a process that is not pwsh, so
        /// point the engine's DllImports at the installation directly.
        /// </summary>
        private static void InstallNativeResolver(Assembly engine, string home)
        {
            try
            {
                NativeLibrary.SetDllImportResolver(engine, (library, assembly, path) =>
                {
                    foreach (var name in NativeNames(library))
                    {
                        var candidate = Path.Combine(home, name);
                        if (File.Exists(candidate) && NativeLibrary.TryLoad(candidate, out var handle)) return handle;
                    }
                    return IntPtr.Zero;
                });
            }
            catch (InvalidOperationException)
            {
                // A resolver is already set for this assembly (the engine was
                // resolved twice, e.g. after a reload): the first one stands.
            }
        }

        private static IEnumerable<string> NativeNames(string library)
        {
            yield return library;
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                if (!library.EndsWith(".dll", StringComparison.OrdinalIgnoreCase)) yield return library + ".dll";
                yield break;
            }
            var extension = RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? ".dylib" : ".so";
            if (!library.EndsWith(extension, StringComparison.OrdinalIgnoreCase))
            {
                yield return library + extension;
                if (!library.StartsWith("lib", StringComparison.Ordinal)) yield return "lib" + library + extension;
            }
        }
    }
}
