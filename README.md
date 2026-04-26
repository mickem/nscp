# NSClient++ (nscp)

[![Build release](https://github.com/mickem/nscp/actions/workflows/build-main.yml/badge.svg)](https://github.com/mickem/nscp/actions/workflows/build-main.yml)


NSClient++ (nscp) aims to be a simple yet powerful and secure monitoring daemon. 
It was built for Nagios/Icinga, but nothing in the daemon is Nagios/Icinga specific and it can be used in many other scenarios where you want to receive/distribute check metrics.

The daemon has 3 main functions:

* Allow a remote machine (monitoring server) to request commands to be run on this machine (the monitored machine) which return the status of the machine.
* Submit the same results to a remote (monitoring server).
* Take action and perform tasks.

NSClient++ can be found at: http://nsclient.org

Documentation can be found at: http://docs.nsclient.org

## Extending NSClient++

NSClient++ is designed to be open ended and allow you to customize it in any way you design thus extensibility is a core feature.

 * ExternalScripts responds to queries and are executed by the operating system and the results are returned as-is.
   This is generally the simplest way to extend NSClient++ as you can utilize whatever infrastructure or skill set you already have.
 * LuaScripts are internal scripts which runs inside NSClient++ and performs various tasks and/or responds to queries.
   This is the best option if you want to allow the script to run on any platform with as little infrastructure as possible.
 * PythonScripts are internal scripts which runs inside NSClient++ and performs various tasks and/or responds to queries.
   Python is an easy and powerful language but it requires you to also install python which is often not possible on server hardware.
 * .Net modules similar to Native modules below but written on the dot-net platform.
   This allows you to write components on top of the large dot-net ecosystem.
 * Modules are native plugins which can extend NSClient++ in pretty much any way possible.
   This is probably the most complicated way but gives you the most power and control.

## Talking to NSClient++

Since NSClient++ is meaningless by itself it also supports a lot of protocols to allow it to be used by a lot of monitoring solutions.

 * NRPE (Nagios Remote plugin Executor) is a Nagios centric protocol to collect remote metrics.
 * NSCA (Nagios Service Check Acceptor) is a Nagios centric protocol for submitting results.
 * NSCP is the native NSClient++ protocol (still under development)
 * dNSCP is a high performance distributed version of NSCP for high volume traffic.
 * NRDP is a replacement for NSCA.
 * check_mk is a protocol utilized by the check_mk monitoring system.
 * Syslog is a protocol primarily designed for submitting log records.
 * Graphite allows you do real-time graphing.
 * SMTP is more of a toy currently.

## Supported OS/Platform

NSClient++ should run on the following operating systems:
 * Windows: In theory from Windows XP with lots of service back (verified Windows 2008 R2 and later)
 * Linux: Debian, Centos and Ubuntu (and possibly others as well)

### Which package to download

The following packages are produced by the official build pipelines. Pick the one that matches your operating system and architecture:

| Operating system               | Version                                           | Architecture     | Package / artifact name                          | Notes                                                                                                                                                            |
|--------------------------------|---------------------------------------------------|------------------|--------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Windows (modern)               | Windows 10 / 11, Server 2016 / 2019 / 2022 / 2025 | x64 (64-bit)     | `NSCP-<version>-x64.msi`                         | Recommended for all modern Windows systems. Built with MSVC 2022.                                                                                                |
| Windows (modern)               | Windows 10 / 11, Server 2016 / 2019 / 2022 / 2025 | x86 (32-bit)     | `NSCP-<version>-Win32.msi`                       | Use only on 32-bit Windows installations. Built with MSVC 2022.                                                                                                  |
| Windows (modern)               | Windows 11 ARM, Server 2025 ARM                   | ARM64            | `NSCP-<version>-ARM64.msi`                       | Native ARM64 build, cross-compiled with the `v143` toolset. Use this on Windows-on-ARM devices (e.g. Surface Pro X / Copilot+ PCs, ARM-based Azure VMs).         |
| Windows (legacy)               | Windows XP and above                              | x86 (32-bit)     | `NSCP-<version>-Win32-legacy-xp.msi`             | Statically linked, built with the `v141_xp` toolset. Use this on any Windows older than Windows 10 / Server 2016. Works on x64 versions of these older OSes too. |
| Ubuntu                         | 24.04 LTS (Noble)                                 | x64 (amd64)      | `NSCP-<version>-ubuntu-24.04-amd64.deb`          | Should also install on recent Debian/Ubuntu derivatives with compatible glibc and Lua 5.4.                                                                       |
| Ubuntu                         | 24.04 LTS (Noble)                                 | ARM64 (aarch64)  | `NSCP-<version>-ubuntu-24.04-arm64.deb`          | For 64-bit ARM hosts (e.g. AWS Graviton, Ampere Altra, Raspberry Pi 4/5 running Ubuntu 24.04 arm64).                                                             |
| Rocky Linux / RHEL / AlmaLinux | 9                                                 | x64 (x86_64)     | `NSCP-<version>-rocky-9-x86_64.rpm`              | Compatible with RHEL 9 and other RHEL 9 rebuilds (AlmaLinux 9, Oracle Linux 9, CentOS Stream 9).                                                                 |
| Rocky Linux / RHEL / AlmaLinux | 9                                                 | ARM64 (aarch64)  | `NSCP-<version>-rocky-9-aarch64.rpm`             | 64-bit ARM build for RHEL 9-family distributions.                                                                                                                |
| Rocky Linux / RHEL / AlmaLinux | 10                                                | x64 (x86_64)     | `NSCP-<version>-rocky-10-x86_64.rpm`             | Compatible with RHEL 10 and other RHEL 10 rebuilds.                                                                                                              |
| Rocky Linux / RHEL / AlmaLinux | 10                                                | ARM64 (aarch64)  | `NSCP-<version>-rocky-10-aarch64.rpm`            | 64-bit ARM build for RHEL 10-family distributions.                                                                                                               |

In addition, a stand-alone `check_nsclient` binary is published alongside each Linux package for use as a Nagios/Icinga check plugin.

> Note: On unsupported distributions you can build from source — see [build.md](build.md).

## Building NSClient++

NSClient++ is built using CMake and Visual Studio 2022.
You can find detailed instructions for building locally in the [build.md](build.md) file.

### Powered by
[![JetBrains logo.](https://resources.jetbrains.com/storage/products/company/brand/logos/jetbrains.svg)](https://jb.gg/OpenSource)

