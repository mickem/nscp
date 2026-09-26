// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The hardware readers and the host facts on Linux: sysfs for batteries,
// thermal zones and cpufreq, and /etc/os-release, DMI and /proc/cpuinfo for
// the facts. The parsing of each is platform-neutral and lives with its
// check; this file only says where Linux keeps it.

#include <sys/utsname.h>
#include <unistd.h>

#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "check_battery.h"
#include "check_cpu_frequency.h"
#include "check_temperature.h"
#include "system_facts.h"

battery_check::batteries_type battery_check::read_battery() { return read_battery_from("/sys/class/power_supply"); }

temperature_check::zones_type temperature_check::read_temperature() { return read_temperature_from("/sys/class/thermal", "/sys/class/hwmon"); }

cpu_frequency_check::cpus_type cpu_frequency_check::read_cpu_frequency() { return read_cpu_frequency("/sys/devices/system/cpu"); }

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

}  // namespace

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
