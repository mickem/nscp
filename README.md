# NSClient++

[![Documentation](https://img.shields.io/badge/docs-docs.nsclient.org-blue)](https://docs.nsclient.org)
[![Latest release](https://img.shields.io/github/v/release/mickem/nscp?include_prereleases&label=release)](https://github.com/mickem/nscp/releases)
[![License](https://img.shields.io/badge/license-Apache--2.0%20OR%20GPL--2.0--only-green.svg)](COPYING)
[![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux-lightgrey)](#supported-platforms)

> A simple, powerful, and secure monitoring agent for Windows and Linux.

NSClient++ (`nsclient`) is the monitoring agent that runs on the machines you want
to monitor. It exposes the system to your monitoring server (Nagios, Icinga,
Naemon, Op5 Monitor, Checkmk, Prometheus, Zabbix-via-NRPE, …) over whichever
protocol that server speaks. It can also push results, expose a REST API,
scrape Windows performance counters, run external scripts, and be extended
with Lua, Python, or native plugins.

- **Website:** <https://nsclient.org>
- **Documentation:** <https://docs.nsclient.org>
- **Issues / discussions:** <https://github.com/mickem/nscp/issues>

---

## Table of contents

- [Quick start](#quick-start)
- [What it does](#what-it-does)
- [Supported protocols](#supported-protocols)
- [Common monitoring scenarios](#common-monitoring-scenarios)
- [Extending NSClient++](#extending-nsclient)
- [Supported platforms](#supported-platforms)
- [Which package to download](#which-package-to-download)
- [Building from source](#building-from-source)
- [Documentation map](#documentation-map)
- [Contributing](#contributing)
- [License](#license)

---

## Quick start

The fastest path from zero to a working check is the
**[Quick Start guide](https://docs.nsclient.org/quick-start/)** — it gets you
installed and running your first check in about 10 minutes.

For more depth:

- [Installing NSClient++](https://docs.nsclient.org/setup/installing/) — interactive MSI walkthrough, silent install, MSI properties, remote config.
- [Web interface](https://docs.nsclient.org/setup/web-interface/) — manage and query the agent from a browser.
- [Securing NSClient++](https://docs.nsclient.org/setup/securing/) — TLS, two-way certificate authentication, per-protocol hardening.
- [How it works (concepts)](https://docs.nsclient.org/concepts/) — modules, commands, the filter/threshold engine shared by every check.
- [FAQ](https://docs.nsclient.org/faq/) — timeouts, allowed-hosts, NRPE insecure mode, performance counter pitfalls, escaping.

---

## What it does

NSClient++ has three core jobs:

1. **Answer queries** — let a monitoring server ask "is this machine healthy?" and return a Nagios-style status + perfdata.
2. **Submit results** — push the same kind of results to a monitoring server on a schedule.
3. **Act on events** — run actions (scripts, custom modules, REST calls) when something changes.

Every check shares the same filter/threshold/perf-config engine, so the same
expressions work across CPU, disk, services, event logs, counters, and custom
scripts. See [Checks in depth](https://docs.nsclient.org/concepts/checks/).

---

## Supported protocols

NSClient++ deliberately speaks many protocols so it can plug into whatever
monitoring stack you already have.

| Protocol         | Direction        | Use it for                                                                          | Guide                                                                                                |
|------------------|------------------|-------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------|
| **NRPE**         | Active (pull)    | Nagios / Icinga / Naemon polling the agent — the most widely supported pattern.     | [Active monitoring with NRPE](https://docs.nsclient.org/scenarios/nrpe/)                             |
| **NSCA**         | Passive (push)   | Legacy Nagios passive submissions.                                                  | [Passive monitoring (NSCA/NRDP)](https://docs.nsclient.org/scenarios/passive-monitoring-nsca/)       |
| **NSCA-NG**      | Passive (push)   | TLS-PSK successor to NSCA — modern crypto, same passive pattern.                    | [Passive monitoring (NSCA-NG)](https://docs.nsclient.org/scenarios/passive-monitoring-nsca-ng/)      |
| **NRDP**         | Passive (push)   | HTTP-based modern replacement for NSCA.                                             | [NRDP (in the NSCA scenario)](https://docs.nsclient.org/scenarios/passive-monitoring-nsca/#using-nrdp-instead-of-nsca) |
| **Icinga 2 API** | Passive (push)   | Submit scheduled check results directly to the Icinga 2 REST API.                   | [Passive monitoring (Icinga 2)](https://docs.nsclient.org/scenarios/passive-monitoring-icinga/)      |
| **Checkmk**      | Active (pull)    | Serve a Checkmk-compatible agent dump on TCP/6556.                                  | [Checkmk agent integration](https://docs.nsclient.org/scenarios/check-mk/)                           |
| **Prometheus**   | Active (scrape)  | Expose OpenMetrics on `/api/v2/openmetrics` for Prometheus to scrape.               | [Prometheus scraping](https://docs.nsclient.org/scenarios/prometheus/)                               |
| **Graphite**     | Passive (push)   | Stream performance data to a Graphite / Carbon backend for graphing.                | [`GraphiteClient` reference](https://docs.nsclient.org/reference/client/GraphiteClient/)             |
| **Syslog**       | Passive (push)   | Forward results as syslog records.                                                  | [`SyslogClient` reference](https://docs.nsclient.org/reference/client/SyslogClient/)                 |
| **SMTP**         | Passive (push)   | Email notifications from check results.                                             | [`SMTPClient` reference](https://docs.nsclient.org/reference/client/SMTPClient/)                     |
| **REST API**     | Active (pull)    | Custom integrations, scripts, dashboards, and the built-in web UI.                  | [REST API reference](https://docs.nsclient.org/api/rest/)                                            |

---

## Common monitoring scenarios

End-to-end guides — each one has the minimal config, the command to run, and
example output. Full list: [docs.nsclient.org/scenarios](https://docs.nsclient.org/scenarios/).

**System health**

- [Windows server health](https://docs.nsclient.org/scenarios/windows-server-health/) — CPU, memory, disk, uptime as a baseline
- [Disk space alerting](https://docs.nsclient.org/scenarios/disk-space/)
- [Service & process monitoring](https://docs.nsclient.org/scenarios/service-monitoring/)
- [Event log monitoring](https://docs.nsclient.org/scenarios/event-log/)
- [Performance counter (PDH) monitoring](https://docs.nsclient.org/scenarios/counters/)

**Network**

- [Network checks](https://docs.nsclient.org/scenarios/network-checks/) — ping, TCP port, HTTP, DNS

**Extensibility**

- [External scripts](https://docs.nsclient.org/scenarios/external-scripts/) — wrap PowerShell, batch, or VBScript checks

---

## Extending NSClient++

NSClient++ is designed to be open-ended. Pick the extension model that fits
your environment:

| Option              | Best for                                                                                                                 |
|---------------------|--------------------------------------------------------------------------------------------------------------------------|
| **ExternalScripts** | Reuse PowerShell, batch, shell, or any existing tooling. Simplest path. [Guide](https://docs.nsclient.org/scenarios/external-scripts/) |
| **LuaScripts**      | In-process scripts with no extra runtime to install — runs anywhere NSClient++ does.                                     |
| **PythonScripts**   | Full Python inside the agent; great power, but you need Python installed on the host. [Guide](https://docs.nsclient.org/extending/python/) |
| **Native modules**  | C++ plugins using the [plugin API](https://docs.nsclient.org/extending/plugin-api/) — maximum control, maximum effort.   |
| **Zip modules**     | Bundle scripts + config as a redistributable add-on. [Guide](https://docs.nsclient.org/extending/zip-modules/)           |

See the [Extending NSClient++](https://docs.nsclient.org/extending/) section
for the full picture.

---

## Supported platforms

- **Windows**: Windows 10 / 11 and Windows Server 2016 / 2019 / 2022 / 2025
  on x64, x86, and ARM64. A legacy build covers Windows XP / Server 2003
  through Server 2012 R2.
- **Linux**: Ubuntu 24.04 LTS and Rocky / RHEL / AlmaLinux 9 and 10 on x86_64
  and aarch64. Other glibc-compatible distributions usually work too. Some
  modules are Windows-only (event log, PDH, WMI, service control).

### Which package to download

The following packages are produced by the official build pipelines. Pick the
one that matches your operating system and architecture:

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

In addition, a stand-alone `check_nsclient` binary is published alongside each
Linux package for use as a Nagios/Icinga check plugin.

> On unsupported distributions you can build from source — see [build.md](build.md).

---

## Building from source

NSClient++ is built with CMake. On Windows it uses Visual Studio 2022; on
Linux it uses GCC or Clang. See [build.md](build.md) for the full step-by-step
build instructions, dependencies, and tips.

---

## Documentation map

The [full documentation](https://docs.nsclient.org) is organised as:

- **[Quick Start](https://docs.nsclient.org/quick-start/)** — your first 10 minutes
- **[Setup](https://docs.nsclient.org/setup/installing/)** — installing, web UI, hardening
- **[Concepts](https://docs.nsclient.org/concepts/)** — how modules, commands, checks, permissions, and settings fit together
- **[Scenarios](https://docs.nsclient.org/scenarios/)** — end-to-end recipes
- **[Reference](https://docs.nsclient.org/reference/)** — every module, command, and setting
- **[Extending](https://docs.nsclient.org/extending/)** — Python, Lua, native plugins, zip modules
- **[REST API](https://docs.nsclient.org/api/rest/)** — programmatic access
- **[FAQ](https://docs.nsclient.org/faq/)** — operational gotchas

---

## Contributing

Contributions are welcome — bug reports, fixes, new checks, doc improvements,
and platform packaging help.

- File issues and feature requests at <https://github.com/mickem/nscp/issues>.
- Pull requests should target `main`. CI builds Windows and Linux packages on
  every PR; please make sure the build is green before requesting review.
- Documentation lives under [`docs`](docs) and is published to
  <https://docs.nsclient.org>.
- For non-trivial changes, open an issue first to discuss the approach.

## License

NSClient++ is dual-licensed — you may use it under **either** the
**Apache License 2.0** **or** the **GNU General Public License, version 2.0 (only)**,
at your option (SPDX: `Apache-2.0 OR GPL-2.0-only`). See
[COPYING](COPYING) for the summary, the [`LICENSES/`](LICENSES) directory for the
full license texts, and [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for
bundled third-party components.

### Powered by
[![JetBrains logo.](https://resources.jetbrains.com/storage/products/company/brand/logos/jetbrains.svg)](https://jb.gg/OpenSource)
