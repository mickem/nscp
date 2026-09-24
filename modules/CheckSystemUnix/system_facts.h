// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#ifndef NSCP_SYSTEM_FACTS_H
#define NSCP_SYSTEM_FACTS_H

#include <facts/host_facts.hpp>
#include <string>

namespace system_facts {

// Build the facts row from already-read inputs. Pure, exposed for unit tests:
// every value that needs deciding is decided here, and gather() below only
// reads the files and calls uname.
//
// `os_release` is the raw contents of /etc/os-release, `sys_vendor` and
// `product_name` the SMBIOS strings from /sys/class/dmi/id (empty where the
// kernel exposes none, as on a container or a non-DMI board), `cpuinfo_flags`
// the `flags` line of /proc/cpuinfo, and the rest comes from uname(2) and the
// sysconf counters.
host_facts::facts build(const std::string &os_release, const std::string &uname_sysname, const std::string &uname_release, const std::string &uname_machine,
                        const std::string &hostname, const std::string &sys_vendor, const std::string &product_name, const std::string &cpuinfo_flags,
                        long cpu_cores, unsigned long long memory_bytes);

// Gather this host's facts. Every source is a local file, uname(2) or
// sysconf(3) - nothing forks, and nothing calls the resolver, because this
// runs on the module load path where a host with an unreachable nameserver
// would otherwise hold up the whole service start. That is why `domain` is
// derived from the configured host name and stays empty when that name is
// unqualified, rather than being resolved the way check_hostname does on
// demand.
host_facts::facts gather();

}  // namespace system_facts

#endif  // NSCP_SYSTEM_FACTS_H
