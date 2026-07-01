# Supported platforms

<!-- @formatter:off -->
!!! note "Support policy is being formalized"
    The matrix below reflects the current state. The exact set of supported versions, the cadence of legacy builds, and the long-term plan for Windows XP support are still being finalised and may change.
<!-- @formatter:on -->

NSClient++ is built for two operating system families: Windows and Linux.

## Windows

There are two Windows editions:

| Edition  | Supported on                                  | Toolset    | Installer           | Notes                                                                                                                                                |
|----------|-----------------------------------------------|------------|---------------------|------------------------------------------------------------------------------------------------------------------------------------------------------|
| Standard | Windows 10 / Server 2016 and later            | `v143`     | MSI (recommended)   | The mainline build. New features and fixes land here first.                                                                                          |
| Legacy   | Windows XP (latest Service Pack), Server 2003 | `v141_xp`  | Manual install only | Best-effort build for older systems. The MSI installer is not supported on XP; the binaries need to be deployed and registered as a service by hand. |

Both 32-bit and 64-bit builds are produced for the standard edition; pick the one that matches the host architecture.

The standard edition is built with the `v143` platform toolset (MSVC 2022),
which raises the minimum supported OS to Windows 10 / Server 2016. Systems
older than that — down to Windows XP — are served by the separate **legacy**
edition, which is built with the `v141_xp` toolset. That toolset is the last
one from Microsoft that can target Windows XP SP3 / Server 2003; Visual Studio
2019 and later dropped Windows XP targeting entirely (see Microsoft's
[Configuring Programs for Windows XP][winxp]).

[winxp]: https://learn.microsoft.com/en-us/cpp/build/configuring-programs-for-windows-xp?view=msvc-170

### Where the Windows 10 / Server 2016 floor comes from

The `v143` limit is not a hard property of the compiler itself — it comes from
the **Visual C++ runtime (redistributable)** that the standard build links
against dynamically. The current Visual C++ Redistributable that ships with
MSVC 2022 requires Windows 10 / Server 2016 or later; Microsoft dropped
Windows 7 SP1 / 8.1 support from the newer redistributable builds. Because the
standard MSI bundles and depends on that current runtime, Windows 10 /
Server 2016 is the supported floor.

<!-- @formatter:off -->
!!! tip "Bring-your-own-redistributable (unsupported)"
    The `v143` *compiler* can still emit code that runs on older Windows
    (down to Windows 7 SP1 / Server 2008 R2 SP1). If you deploy the standard
    build alongside an **older** Visual C++ Redistributable — one that still
    supports Windows 7 SP1 / 8.1 (for example a `14.3x` redistributable rather
    than the latest) — it will **likely** run on those older systems.

    This is *not tested or supported*: we build and validate against the
    current runtime only. If you need a genuinely supported path for
    pre-Windows-10 systems, use the **legacy** (`v141_xp`) edition instead.
    See Microsoft's [latest supported VC++ redistributable requirements][vcredist]
    for which OS versions each redistributable build supports.
<!-- @formatter:on -->

[vcredist]: https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170

## Linux

NSClient++ runs on most modern Linux distributions. There is no single "minimum version" — what matters is that the C++ runtime and OpenSSL versions in the distribution are recent enough for the packaged build to load. The releases page lists packages for the distributions that are tested for each release.

## Architectures

* Windows: x86 (32-bit) and x64 (64-bit)
* Linux: x86_64 (others may build from source)

## Choosing an edition on Windows

If the target machine runs Windows 10 / Server 2016 or newer, use the **standard** edition — that is the only edition that receives new features.

The **legacy** edition only exists so that older estates (typically Windows XP and Server 2003, up to Windows 8.1 / Server 2012 R2) can still report into a modern monitoring server. New deployments should not start on the legacy edition.
