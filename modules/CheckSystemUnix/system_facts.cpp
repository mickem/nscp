// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "system_facts.h"

#include <sys/utsname.h>
#include <unistd.h>

#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
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

// Read a whole (small, virtual) file. Returns an empty string when it is not
// there, which is the normal case for the DMI files on a container or a board
// with no SMBIOS.
std::string read_file(const std::string &path) {
  std::ifstream ifs(path.c_str());
  if (!ifs.is_open()) return "";
  std::stringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

// The first `flags` (x86) or `Features` (ARM) line of /proc/cpuinfo.
std::string read_cpuinfo_flags() {
  std::ifstream ifs("/proc/cpuinfo");
  if (!ifs.is_open()) return "";
  std::string line;
  while (std::getline(ifs, line)) {
    const std::string::size_type colon = line.find(':');
    if (colon == std::string::npos) continue;
    const std::string key = to_lower(trim(line.substr(0, colon)));
    if (key == "flags" || key == "features") return trim(line.substr(colon + 1));
  }
  return "";
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
  host_facts::facts f;

  // `os_family` is the kernel's own name for itself, lower-cased: linux,
  // freebsd, darwin. The Windows module hard-codes "windows" for the same
  // reason - it is the one value uname would give there.
  f.os_family = to_lower(trim(uname_sysname));

  // `os_version` is the kernel version on both platforms (Windows publishes
  // its build number, which is the same thing), so a selector for "hosts
  // still on the old kernel" reads the same everywhere. The distribution and
  // its version are the human-readable half and live in `os_name`.
  f.os_version = trim(uname_release);

  const os_version::os_release_info release = os_version::parse_os_release(os_release);
  if (!release.pretty.empty()) {
    f.os_name = release.pretty;
  } else if (!release.distribution_name.empty()) {
    f.os_name = trim(release.distribution_name + " " + release.version);
  } else {
    // No /etc/os-release at all (a minimal container, a non-Linux unix):
    // the kernel name is a poor product name but it is true.
    f.os_name = trim(uname_sysname);
  }

  f.arch = host_facts::normalize_arch(trim(uname_machine));
  if (cpu_cores > 0) f.cpu_cores = cpu_cores;
  f.memory_gb = host_facts::memory_gb_from_bytes(memory_bytes);

  f.manufacturer = clean_dmi(sys_vendor);
  f.model = clean_dmi(product_name);

  // SMBIOS first, the cpuinfo flag second - the same precedence the Windows
  // module uses, and for the same reason: the `hypervisor` flag is that
  // platform's hypervisor bit, and a machine whose firmware names an OEM and
  // a product is the physical host whatever is running on top of it.
  f.virtualization = host_facts::virtualization_from_dmi(f.manufacturer, f.model);
  if (f.virtualization.empty() && host_facts::dmi_names_physical_hardware(f.manufacturer, f.model)) {
    f.virtualization = "none";
  }
  if (f.virtualization.empty() && has_hypervisor_flag(cpuinfo_flags)) {
    // Virtualized, but by something the SMBIOS strings do not name. Better an
    // honest "some hypervisor" than a wrong "none": the common selector is
    // "is this bare metal", and that must not match a VM.
    f.virtualization = "virtual";
  }
  if (f.virtualization.empty() && !cpuinfo_flags.empty()) {
    // The flags line was readable and carried no hypervisor bit, and no DMI
    // string named one. Only then is bare metal a conclusion rather than a
    // guess - an unreadable /proc/cpuinfo leaves the fact unknown.
    f.virtualization = "none";
  }

  f.domain = host_facts::domain_from_fqdn(trim(hostname));

  return f;
}

host_facts::facts gather() {
  utsname uts;
  std::memset(&uts, 0, sizeof(uts));
  const bool have_uname = ::uname(&uts) == 0;

  char hostname[256] = {0};
  if (::gethostname(hostname, sizeof(hostname) - 1) != 0) hostname[0] = '\0';

  long cores = ::sysconf(_SC_NPROCESSORS_ONLN);
  if (cores < 0) cores = 0;

  unsigned long long memory_bytes = 0;
  const long pages = ::sysconf(_SC_PHYS_PAGES);
  const long page_size = ::sysconf(_SC_PAGESIZE);
  if (pages > 0 && page_size > 0) memory_bytes = static_cast<unsigned long long>(pages) * static_cast<unsigned long long>(page_size);

  // /etc/os-release is the documented location; /usr/lib/os-release is the
  // vendor copy a stateless or read-only-root system ships instead.
  std::string os_release = read_file("/etc/os-release");
  if (os_release.empty()) os_release = read_file("/usr/lib/os-release");

  return build(os_release, have_uname ? uts.sysname : "", have_uname ? uts.release : "", have_uname ? uts.machine : "", hostname,
               read_file("/sys/class/dmi/id/sys_vendor"), read_file("/sys/class/dmi/id/product_name"), read_cpuinfo_flags(), cores, memory_bytes);
}

}  // namespace system_facts
