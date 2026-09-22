# Supported platforms

<!-- @formatter:off -->
!!! note "Support policy is being formalized"
    The matrix below reflects the current state. The exact set of supported versions, the cadence of legacy builds, and the long-term plan for Windows XP support are still being finalised and may change.
<!-- @formatter:on -->

NSClient++ is built for three operating system families: Windows, Linux and
macOS.

## Windows

There are two Windows editions:

| Edition  | Supported on                                  | Installer           | Notes                                                                                                                                                |
|----------|-----------------------------------------------|---------------------|------------------------------------------------------------------------------------------------------------------------------------------------------|
| Standard | Windows Server 2008 R2 / Windows 7 and later  | MSI (recommended)   | The mainline build. New features and fixes land here first.                                                                                          |
| Legacy   | Windows XP (latest Service Pack), Server 2003 | Manual install only | Best-effort build for older systems. The MSI installer is not supported on XP; the binaries need to be deployed and registered as a service by hand. |

Both 32-bit and 64-bit builds are produced for the standard edition; pick the one that matches the host architecture.

## Linux

NSClient++ runs on most modern Linux distributions. There is no single "minimum version" — what matters is that the C++ runtime and OpenSSL versions in the distribution are recent enough for the packaged build to load. The releases page lists packages for the distributions that are tested for each release.

## macOS

<!-- @formatter:off -->
!!! warning "Preview"
    The macOS build is new. It installs and runs as a launchd daemon, but it
    ships a smaller set of check modules than Windows and Linux - see below.
<!-- @formatter:on -->

macOS builds are **Apple silicon (arm64) only** and are published as a `.pkg`
installer plus a tarball of the same tree; both bundle their dependencies, so
neither needs Homebrew on the target. The minimum macOS version is the one the
release was built on, because the package carries the dependency libraries
built for it; the installer refuses anything older rather than failing at
launch. There is no Intel build, and Rosetta 2 does not substitute for one.

The packages are not yet signed with a Developer ID or notarized, so Gatekeeper
blocks a double-click install. Install from the command line
(`sudo installer -pkg ... -target /`) or right-click and choose *Open*.

Three modules are not built for macOS, because their data sources are Linux
kernel interfaces: `CheckSystem` (procfs), `CheckDisk` (`mntent` and
`/proc/diskstats`) and `CheckLogFile` (`inotify`). That means no CPU, memory,
process, uptime, service, disk or log-file checks on macOS yet. Everything else
- the REST API and web UI, the NRPE/NSCA/NSCP/check_mk listeners and clients,
external scripts, the Lua and Python script engines, the network and security
checks, the scheduler and the forwarders - is present. See [Installing on
macOS](installing.md#installing-on-macos-pkg) for the full picture.

## Architectures

* Windows: x86 (32-bit) and x64 (64-bit)
* Linux: x86_64 and arm64
* macOS: arm64 (Apple silicon) only

## Choosing an edition on Windows

If the target machine runs Windows 7 / Server 2008 R2 or newer, use the **standard** edition — that is the only edition that receives new features.

The **legacy** edition only exists so that older estates (typically Windows XP and Server 2003) can still report into a modern monitoring server. New deployments should not start on the legacy edition.
