// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// network_facts::gather() for macOS: the interfaces the kernel lists, with
// their hardware address, link state and speed from the same NET_RT_IFLIST2
// walk check_network uses, and their addresses from getifaddrs(3). Nothing
// forks and nothing touches the resolver (getnameinfo is asked for numeric
// hosts only).

#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/socket.h>

#include <algorithm>
#include <facts/network_facts.hpp>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "interfaces_darwin.h"

namespace {

struct seen_interface {
  bool loopback = false;
  bool listed = false;
  darwin_interfaces::interface_info link;
  std::vector<std::string> addresses;
};

}  // namespace

std::vector<network_facts::interface_record> network_facts::gather() {
  // A std::map, so the records come out sorted by id: the kernel's order is
  // not a stable one, and the same network must be the same document.
  std::map<std::string, seen_interface> seen;
  for (const darwin_interfaces::interface_info &nic : darwin_interfaces::read()) {
    seen_interface &entry = seen[nic.name];
    entry.listed = true;
    entry.loopback = nic.loopback;
    entry.link = nic;
  }

  struct ifaddrs *list = nullptr;
  if (getifaddrs(&list) == 0) {
    for (const struct ifaddrs *entry = list; entry != nullptr; entry = entry->ifa_next) {
      if (entry->ifa_name == nullptr) continue;
      seen_interface &nic = seen[entry->ifa_name];
      if ((entry->ifa_flags & IFF_LOOPBACK) != 0) nic.loopback = true;
      if (entry->ifa_addr == nullptr) continue;
      const int family = entry->ifa_addr->sa_family;
      if (family != AF_INET && family != AF_INET6) continue;
      const socklen_t length = family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
      char host[NI_MAXHOST];
      if (getnameinfo(entry->ifa_addr, length, host, sizeof(host), nullptr, 0, NI_NUMERICHOST) != 0) continue;
      nic.addresses.push_back(normalize_address(host));
    }
    freeifaddrs(list);
  }

  std::vector<interface_record> interfaces;
  for (std::pair<const std::string, seen_interface> &entry : seen) {
    if (entry.second.loopback) continue;
    interface_record nic;
    nic.id = entry.first;
    if (entry.second.listed) {
      nic.mac = normalize_mac(entry.second.link.mac);
      nic.status = entry.second.link.status;
      if (entry.second.link.speed_bps > 0) nic.speed_bps = entry.second.link.speed_bps;
    }
    std::vector<std::string> &addresses = entry.second.addresses;
    // Sorted, so the order the kernel happens to list them in is not a change.
    std::sort(addresses.begin(), addresses.end());
    addresses.erase(std::unique(addresses.begin(), addresses.end()), addresses.end());
    nic.addresses = addresses;
    interfaces.push_back(nic);
  }
  return interfaces;
}
