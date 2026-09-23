// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "facts.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <map>
#include <set>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "check_hostname.h"
#include "check_os_version.h"

namespace check_system_facts {

std::string read_file(const std::string &path) {
  std::ifstream stream(path.c_str());
  if (!stream) return std::string();
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

namespace {

// A single-line /sys or /proc file, trimmed. Most of the DMI table is exactly
// this, and a missing file is the normal case on a VM rather than an error.
std::string read_line(const std::string &path) {
  std::string content = read_file(path);
  const std::string::size_type newline = content.find('\n');
  if (newline != std::string::npos) content.erase(newline);
  boost::algorithm::trim(content);
  // The DMI table is full of these where the board did not program a value.
  // They are not facts, so they are dropped rather than published.
  static const std::set<std::string> placeholders = {"To be filled by O.E.M.",
                                                     "To Be Filled By O.E.M.",
                                                     "Default string",
                                                     "System Serial Number",
                                                     "System manufacturer",
                                                     "System Product Name",
                                                     "Not Specified",
                                                     "Not Applicable",
                                                     "None",
                                                     "Unknown",
                                                     "0123456789"};
  if (placeholders.count(content)) return std::string();
  return content;
}

// SMBIOS chassis type (spec 7.4.1), as /sys/class/dmi/id/chassis_type gives
// it. The same vocabulary the Windows module's chassis_type_to_string uses, so
// a fleet query for laptops matches on both.
std::string chassis_from_dmi(const std::string &raw) {
  if (raw.empty()) return std::string();
  const long long type = strtoll(raw.c_str(), nullptr, 10);
  switch (type) {
    case 3:
      return "desktop";
    case 4:
      return "low_profile_desktop";
    case 6:
      return "mini_tower";
    case 7:
      return "tower";
    case 8:
      return "portable";
    case 9:
      return "laptop";
    case 10:
      return "notebook";
    case 13:
      return "all_in_one";
    case 14:
      return "sub_notebook";
    case 17:
      return "main_server_chassis";
    case 23:
      return "rack_mount_chassis";
    case 28:
      return "blade";
    case 29:
      return "blade_enclosure";
    case 30:
      return "tablet";
    case 31:
      return "convertible";
    case 32:
      return "detachable";
    default:
      return std::string();
  }
}

}  // namespace

void parse_cpuinfo(const std::string &content, std::string &model, long long &sockets, long long &cores) {
  model.clear();
  sockets = 0;
  cores = 0;
  std::set<std::string> physical_ids;
  long long processors = 0;
  long long cores_per_socket = 0;

  std::istringstream stream(content);
  std::string line;
  while (std::getline(stream, line)) {
    const std::string::size_type colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string key = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    boost::algorithm::trim(key);
    boost::algorithm::trim(value);
    if (key == "model name" && model.empty()) {
      model = value;
    } else if (key == "Hardware" && model.empty()) {
      // ARM boards have no "model name"; this is the closest thing they do
      // report, and an empty cpu_model would be worse than an approximate one.
      model = value;
    } else if (key == "processor") {
      ++processors;
    } else if (key == "physical id") {
      physical_ids.insert(value);
    } else if (key == "cpu cores") {
      cores_per_socket = strtoll(value.c_str(), nullptr, 10);
    }
  }

  sockets = static_cast<long long>(physical_ids.size());
  // A kernel that does not report `physical id` (every ARM board, and some
  // VMs) still has at least one socket if it has a processor at all.
  if (sockets == 0 && processors > 0) sockets = 1;
  cores = cores_per_socket > 0 ? cores_per_socket * std::max<long long>(sockets, 1) : processors;
}

unsigned long long parse_meminfo_total(const std::string &content) {
  std::istringstream stream(content);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.compare(0, 9, "MemTotal:") != 0) continue;
    const unsigned long long kilobytes = strtoull(line.c_str() + 9, nullptr, 10);
    // /proc/meminfo is in kibibytes; the document is in bytes, because that is
    // what the key says and what every other size in it means.
    return kilobytes * 1024ull;
  }
  return 0;
}

std::time_t parse_boot_time(const std::string &uptime_content, const std::time_t now) {
  if (uptime_content.empty() || now <= 0) return 0;
  const double seconds = strtod(uptime_content.c_str(), nullptr);
  if (seconds <= 0) return 0;
  return now - static_cast<std::time_t>(seconds);
}

os_facts gather_os(std::string &error) {
  os_facts found;
  found.family = "linux";

  utsname name{};
  if (uname(&name) == -1) {
    error = "could not read the system name (uname failed)";
    return found;
  }
  found.kernel = name.release;
  found.arch = name.machine;

  const os_version::os_release_info release = os_version::read_os_release_from("/etc/os-release");
  found.distribution = release.distribution;
  found.version = release.version;
  // The same fallback check_os_version uses for ${os}, so the fact and the
  // check never disagree about what this machine runs.
  found.name = !release.pretty.empty() ? release.pretty : (std::string(name.sysname) + " " + name.release);

  found.boot_time = parse_boot_time(read_file("/proc/uptime"), ::time(nullptr));
  return found;
}

identity_facts gather_identity(std::string &error) {
  identity_facts found;
  char buffer[256] = {0};
  if (gethostname(buffer, sizeof(buffer) - 1) != 0) {
    error = "could not read the host name (gethostname failed)";
    return found;
  }
  const std::string hostname(buffer);

  // Best-effort canonicalisation, exactly as check_hostname does it: a
  // container or a host without DNS legitimately fails here, and the FQDN then
  // falls back to the bare host name.
  std::string canonical;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_CANONNAME;
  struct addrinfo *resolved = nullptr;
  if (getaddrinfo(hostname.c_str(), nullptr, &hints, &resolved) == 0 && resolved != nullptr) {
    if (resolved->ai_canonname != nullptr) canonical = resolved->ai_canonname;
    freeaddrinfo(resolved);
  }

  // Derived by the same function the check uses, so `identity.fqdn` and the
  // check's `fqdn` keyword are the same string.
  const hostname_check::host_identity identity = hostname_check::derive_identity(hostname, canonical);
  found.hostname = identity.hostname;
  found.fqdn = identity.fqdn;
  found.domain = identity.domain;
  return found;
}

hardware_facts gather_hardware(std::string &error) {
  hardware_facts found;
  const std::string dmi = "/sys/class/dmi/id/";

  found.vendor = read_line(dmi + "sys_vendor");
  found.model = read_line(dmi + "product_name");
  found.serial = read_line(dmi + "product_serial");
  found.asset_tag = read_line(dmi + "chassis_asset_tag");
  found.uuid = read_line(dmi + "product_uuid");
  found.chassis = chassis_from_dmi(read_line(dmi + "chassis_type"));

  parse_cpuinfo(read_file("/proc/cpuinfo"), found.cpu_model, found.cpu_sockets, found.cpu_cores);
  found.memory_total_bytes = parse_meminfo_total(read_file("/proc/meminfo"));

  // The DMI serial and UUID are root-only on most distributions and the CPU
  // and memory come from /proc, so a non-root agent gets a thinner record
  // rather than none. Nothing here is an error worth reporting on the set:
  // "this machine has no DMI table" is the normal case in a container.
  if (found.cpu_model.empty() && found.memory_total_bytes == 0 && found.vendor.empty()) {
    error = "no hardware information available (neither /sys/class/dmi/id nor /proc/cpuinfo could be read)";
  }
  // Memory modules come from the SMBIOS type-17 table, which Linux exposes
  // only through dmidecode (a setuid read of /dev/mem). Left out rather than
  // guessed; the Windows module reports them from WMI.
  return found;
}

std::vector<interface_facts> gather_interfaces(std::string &error) {
  std::vector<interface_facts> interfaces;
  struct ifaddrs *addresses = nullptr;
  if (getifaddrs(&addresses) != 0) {
    error = "could not enumerate network interfaces (getifaddrs failed)";
    return interfaces;
  }

  // getifaddrs returns one entry per interface *and address family*, so the
  // records are accumulated by name and the addresses appended.
  std::vector<std::string> order;
  std::map<std::string, interface_facts> by_name;
  for (struct ifaddrs *entry = addresses; entry != nullptr; entry = entry->ifa_next) {
    if (entry->ifa_name == nullptr) continue;
    const std::string name(entry->ifa_name);
    if (by_name.find(name) == by_name.end()) {
      interface_facts found;
      found.id = name;
      found.mac = read_line("/sys/class/net/" + name + "/address");
      const std::string operstate = read_line("/sys/class/net/" + name + "/operstate");
      found.state = operstate.empty() ? "unknown" : operstate;
      // /sys reports megabits; the key says bits, which is what every other
      // rate in the document means.
      const std::string speed = read_line("/sys/class/net/" + name + "/speed");
      const long long megabits = speed.empty() ? -1 : strtoll(speed.c_str(), nullptr, 10);
      if (megabits > 0) found.speed_bps = megabits * 1000000ll;
      by_name[name] = found;
      order.push_back(name);
    }

    if (entry->ifa_addr == nullptr) continue;
    char text[INET6_ADDRSTRLEN] = {0};
    if (entry->ifa_addr->sa_family == AF_INET) {
      const struct sockaddr_in *in = reinterpret_cast<const struct sockaddr_in *>(entry->ifa_addr);
      if (inet_ntop(AF_INET, &in->sin_addr, text, sizeof(text)) != nullptr) by_name[name].addresses.push_back(text);
    } else if (entry->ifa_addr->sa_family == AF_INET6) {
      const struct sockaddr_in6 *in6 = reinterpret_cast<const struct sockaddr_in6 *>(entry->ifa_addr);
      if (inet_ntop(AF_INET6, &in6->sin6_addr, text, sizeof(text)) != nullptr) by_name[name].addresses.push_back(text);
    }
  }
  freeifaddrs(addresses);

  interfaces.reserve(order.size());
  for (const std::string &name : order) interfaces.push_back(by_name[name]);
  return interfaces;
}

}  // namespace check_system_facts
