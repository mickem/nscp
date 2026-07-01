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
edition, which is built with the `v141_xp` toolset.

## Linux

NSClient++ runs on most modern Linux distributions. There is no single "minimum version" — what matters is that the C++ runtime and OpenSSL versions in the distribution are recent enough for the packaged build to load. The releases page lists packages for the distributions that are tested for each release.

## Architectures

* Windows: x86 (32-bit) and x64 (64-bit)
* Linux: x86_64 (others may build from source)

## Choosing an edition on Windows

If the target machine runs Windows 10 / Server 2016 or newer, use the **standard** edition — that is the only edition that receives new features.

The **legacy** edition only exists so that older estates (typically Windows XP and Server 2003, up to Windows 8.1 / Server 2012 R2) can still report into a modern monitoring server. New deployments should not start on the legacy edition.
