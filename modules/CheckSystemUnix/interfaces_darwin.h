// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// One walk of the Darwin interface list, shared by the collector
// (check_network, the network metrics) and the network.interfaces facts.

#include <string>
#include <vector>

namespace darwin_interfaces {

struct interface_info {
  std::string name;
  std::string mac;  // aa:bb:cc:dd:ee:ff, empty when the interface has no link-layer address
  bool loopback = false;
  // "up", "down" or "unknown" - the RFC 2863 words check_network and the facts
  // document both use.
  std::string status;
  long long speed_bps = 0;
  // 64-bit cumulative counters (if_data64), so they do not wrap at 4 GiB the
  // way the 32-bit getifaddrs(3) copy does.
  unsigned long long rx_bytes = 0, rx_packets = 0, rx_errors = 0;
  unsigned long long tx_bytes = 0, tx_packets = 0, tx_errors = 0;
};

// Every interface the kernel lists, from sysctl NET_RT_IFLIST2. Throws
// std::runtime_error when the sysctl itself fails.
std::vector<interface_info> read();

}  // namespace darwin_interfaces
