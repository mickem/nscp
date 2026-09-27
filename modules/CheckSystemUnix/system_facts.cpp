// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "system_facts.h"

#include <cctype>
#include <string>

#include "check_os_version.h"

namespace system_facts {
namespace {

std::string to_lower(const std::string &s) {
  std::string result = s;
  for (std::string::size_type i = 0; i < result.size(); ++i) {
    result[i] = static_cast<char>(::tolower(static_cast<unsigned char>(result[i])));
  }
  return result;
}

std::string trim(const std::string &s) {
  const std::string::size_type begin = s.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return "";
  const std::string::size_type end = s.find_last_not_of(" \t\r\n");
  return s.substr(begin, end - begin + 1);
}

// Does the flags line announce a hypervisor? The x86 `hypervisor` flag is the
// /proc/cpuinfo rendering of the same CPUID bit the Windows side reads, so
// both platforms conclude "virtualized" from the same signal.
bool has_hypervisor_flag(const std::string &flags) {
  const std::string haystack = " " + to_lower(flags) + " ";
  return haystack.find(" hypervisor ") != std::string::npos;
}

// SMBIOS placeholders OEMs and firmware leave behind. Worse than nothing as a
// fact, because they look like an answer.
bool is_dmi_placeholder(const std::string &value) {
  const std::string lower = to_lower(value);
  return lower.empty() || lower == "to be filled by o.e.m." || lower == "system manufacturer" || lower == "system product name" || lower == "default string" ||
         lower == "not applicable" || lower == "not specified" || lower == "none" || lower == "unknown" || lower == "chassis manufacturer";
}

std::string clean_dmi(const std::string &raw) {
  const std::string value = trim(raw);
  return is_dmi_placeholder(value) ? "" : value;
}

}  // namespace

host_facts::facts build(const std::string &os_release, const std::string &uname_sysname, const std::string &uname_release, const std::string &uname_machine,
                        const std::string &hostname, const std::string &sys_vendor, const std::string &product_name, const std::string &cpuinfo_flags,
                        long cpu_cores, unsigned long long memory_bytes) {
  inputs in;
  in.release = os_version::parse_os_release(os_release);
  in.uname_sysname = uname_sysname;
  in.uname_release = uname_release;
  in.uname_machine = uname_machine;
  in.hostname = hostname;
  in.vendor = sys_vendor;
  in.model = product_name;
  // A readable flags line says one way or the other. An unreadable
  // /proc/cpuinfo leaves the question open.
  if (!cpuinfo_flags.empty()) in.hypervisor = has_hypervisor_flag(cpuinfo_flags);
  in.cpu_cores = cpu_cores;
  in.memory_bytes = memory_bytes;
  return build(in);
}

host_facts::facts build(const inputs &in) {
  host_facts::facts f;

  // `os_family` is the kernel's own name for itself, lower-cased: linux,
  // freebsd, darwin. The Windows module hard-codes "windows" for the same
  // reason - it is the one value uname would give there.
  f.os_family = to_lower(trim(in.uname_sysname));

  // `os_version` is the kernel version on both platforms (Windows publishes
  // its build number, which is the same thing), so a selector for "hosts
  // still on the old kernel" reads the same everywhere. The distribution and
  // its version are the human-readable half and live in `os_name`.
  f.os_version = trim(in.uname_release);

  const os_version::os_release_info &release = in.release;
  if (!release.pretty.empty()) {
    f.os_name = release.pretty;
  } else if (!release.distribution_name.empty()) {
    f.os_name = trim(release.distribution_name + " " + release.version);
  } else {
    // No /etc/os-release at all (a minimal container, a non-Linux unix):
    // the kernel name is a poor product name but it is true.
    f.os_name = trim(in.uname_sysname);
  }

  f.arch = host_facts::normalize_arch(trim(in.uname_machine));
  if (in.cpu_cores > 0) f.cpu_cores = in.cpu_cores;
  f.memory_gb = host_facts::memory_gb_from_bytes(in.memory_bytes);

  f.manufacturer = clean_dmi(in.vendor);
  f.model = clean_dmi(in.model);

  // SMBIOS first, the CPU second - the same precedence the Windows module
  // uses, and for the same reason: the CPU's hypervisor bit is set on a host
  // running Hyper-V or VBS too, and a machine whose firmware names an OEM and
  // a product is the physical host whatever is running on top of it.
  f.virtualization = host_facts::virtualization_from_dmi(f.manufacturer, f.model);
  if (f.virtualization.empty() && host_facts::dmi_names_physical_hardware(f.manufacturer, f.model)) {
    f.virtualization = "none";
  }
  if (f.virtualization.empty() && in.hypervisor) {
    // Virtualized by something the SMBIOS strings do not name: better an
    // honest "some hypervisor" than a wrong "none", since the common selector
    // is "is this bare metal", and that must not match a VM. Bare metal is a
    // conclusion only when the CPU could be asked; otherwise the fact stays
    // unknown.
    f.virtualization = in.hypervisor.value() ? "virtual" : "none";
  }

  f.domain = host_facts::domain_from_fqdn(trim(in.hostname));

  return f;
}

}  // namespace system_facts
