# Supported platforms

<!-- @formatter:off -->
!!! note "Support policy is being formalized"
    The matrix below reflects the current state. The exact set of supported versions, the cadence of legacy builds, and the long-term plan for Windows XP support are still being finalised and may change.
<!-- @formatter:on -->

NSClient++ is built for two operating system families: Windows and Linux.

## Windows

There are two Windows editions:

| Edition  | Runs on                                       | Build       | How to install      | Notes                                                                                                                                        |
|----------|-----------------------------------------------|-------------|---------------------|----------------------------------------------------------------------------------------------------------------------------------------------|
| Standard | Windows 10 / Server 2016 and later            | `v143`      | MSI (recommended)   | The mainline build. New features and fixes land here first.                                                                                  |
| Legacy   | Windows XP (latest Service Pack), Server 2003 | `v141_xp`   | Manual install only | For older machines. The MSI installer will not run on XP, so the files have to be copied in and registered as a service by hand.             |

Both 32-bit and 64-bit installers are produced for the standard edition; pick
the one that matches the machine.

The **standard** edition is the one to use on any supported version of Windows
(Windows 10 / Server 2016 and newer). The **legacy** edition exists only for
older machines — anything from Windows XP up to Windows 8.1 / Server 2012 R2 —
that cannot be upgraded but still need to report into your monitoring server.

(The `v143` / `v141_xp` labels are the Microsoft build tools each edition is
compiled with. Windows XP support was removed from Microsoft's tools after
Visual Studio 2017, which is why the legacy edition stays on the older
`v141_xp` tools — see Microsoft's [Configuring Programs for Windows XP][winxp].)

[winxp]: https://learn.microsoft.com/en-us/cpp/build/configuring-programs-for-windows-xp?view=msvc-170

### Why the standard edition needs Windows 10 / Server 2016

The version requirement does not really come from NSClient++ itself — it comes
from the **Microsoft Visual C++ Runtime** that the program needs in order to
start. The standard installer ships and installs the current version of that
runtime, and Microsoft only supports it on Windows 10 / Server 2016 and newer
(support for Windows 7 SP1 / 8.1 was dropped from the current runtime). That is
what sets the minimum version.

<!-- @formatter:off -->
!!! tip "Getting the standard build onto older Windows (unsupported)"
    In practice the standard build will often still start on older machines
    (down to Windows 7 SP1 / Server 2008 R2 SP1) — but only if an **older**
    version of the Microsoft Visual C++ Redistributable is already installed,
    one that still supports those systems (for example a `14.3x` release rather
    than the newest). If only the current runtime is present it refuses to
    install on those older systems and NSClient++ will not start.

    We do not test or support this: the standard build is only validated
    against the current runtime. If you need a properly supported option for
    anything older than Windows 10 / Server 2016, use the **legacy** edition
    instead. Microsoft lists which Windows versions each runtime release
    supports on their [Visual C++ Redistributable download page][vcredist].
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
