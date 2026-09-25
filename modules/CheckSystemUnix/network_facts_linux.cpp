// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// network_facts::gather() for Linux: the interfaces the kernel lists in
// /sys/class/net, with their addresses from getifaddrs(3). Every source is a
// sysfs file or a netlink dump the C library does for us - nothing forks and
// nothing touches the resolver (getnameinfo is asked for numeric hosts only).

#include <dirent.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/socket.h>

#include <algorithm>
#include <exception>
#include <facts/network_facts.hpp>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

// The first line of a sysfs attribute, or empty when it cannot be read - some
// attributes (speed on a link that is down, on a virtual device) exist but
// fail to read, which is "unknown", not an error.
std::string read_attribute(const std::string &path) {
  std::ifstream in(path.c_str());
  std::string line;
  if (!in || !std::getline(in, line)) return "";
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) line.pop_back();
  return line;
}

// ARPHRD_LOOPBACK, from /sys/class/net/<if>/type. Read from sysfs rather than
// trusting the name: nothing requires the loopback to be called `lo`.
bool is_loopback(const std::string &base) { return read_attribute(base + "type") == "772"; }

struct seen_interface {
  bool loopback = false;
  std::vector<std::string> addresses;
};

// Every interface getifaddrs knows, with the addresses on it. The kernel
// lists an interface without an address too (AF_PACKET on Linux, AF_LINK on
// the BSDs and macOS), so this is also the list of interfaces on a system
// that has no /sys/class/net.
std::map<std::string, seen_interface> read_interfaces() {
  std::map<std::string, seen_interface> interfaces;
  struct ifaddrs *list = nullptr;
  if (getifaddrs(&list) != 0) return interfaces;
  for (const struct ifaddrs *entry = list; entry != nullptr; entry = entry->ifa_next) {
    if (entry->ifa_name == nullptr) continue;
    seen_interface &nic = interfaces[entry->ifa_name];
    if ((entry->ifa_flags & IFF_LOOPBACK) != 0) nic.loopback = true;
    if (entry->ifa_addr == nullptr) continue;
    const int family = entry->ifa_addr->sa_family;
    if (family != AF_INET && family != AF_INET6) continue;
    const socklen_t length = family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
    char host[NI_MAXHOST];
    if (getnameinfo(entry->ifa_addr, length, host, sizeof(host), nullptr, 0, NI_NUMERICHOST) != 0) continue;
    nic.addresses.push_back(network_facts::normalize_address(host));
  }
  freeifaddrs(list);
  return interfaces;
}

}  // namespace

std::vector<network_facts::interface_record> network_facts::gather() {
  std::map<std::string, seen_interface> seen = read_interfaces();

  // An interface with no address and no link-layer entry still has a sysfs
  // directory (a bridge port, a link that is administratively down).
  const std::string root = "/sys/class/net/";
  if (DIR *dir = opendir(root.c_str())) {
    while (const struct dirent *entry = readdir(dir)) {
      const std::string name = entry->d_name;
      if (!name.empty() && name[0] != '.') seen[name];
    }
    closedir(dir);
  }

  // A std::map, so the records come out sorted by id: readdir and getifaddrs
  // order is not a stable one, and the same network must be the same
  // document.
  std::vector<interface_record> interfaces;
  for (std::pair<const std::string, seen_interface> &entry : seen) {
    const std::string &name = entry.first;
    const std::string base = root + name + "/";
    if (entry.second.loopback || is_loopback(base)) continue;

    interface_record nic;
    nic.id = name;
    // The metadata is sysfs, so a system without it lists the interface with
    // its addresses and nothing else, rather than not at all.
    nic.mac = normalize_mac(read_attribute(base + "address"));
    nic.status = read_attribute(base + "operstate");
    // Mbit/s; -1 (or a read error) on a link that is down or has no notion
    // of speed, both of which are "unknown".
    try {
      const long long mbit = std::stoll(read_attribute(base + "speed"));
      if (mbit > 0) nic.speed_bps = mbit * 1000000LL;
    } catch (const std::exception &) {
    }
    std::vector<std::string> &addresses = entry.second.addresses;
    // Sorted, so the order the kernel happens to dump them in is not a change.
    std::sort(addresses.begin(), addresses.end());
    addresses.erase(std::unique(addresses.begin(), addresses.end()), addresses.end());
    nic.addresses = addresses;
    interfaces.push_back(nic);
  }
  return interfaces;
}
