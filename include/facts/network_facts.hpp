// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <ctime>
#include <string>
#include <vector>

namespace nscapi {
namespace facts {
class response;
}
}  // namespace nscapi

// The `network` fact set: the host's network interfaces, what they are called,
// their hardware address, link state and speed, and the addresses on them.
//
// The inventory, not the monitoring: no byte or packet counters. Those move
// every second, and a value that moves every round would bump the document's
// revision every round - check_network and the metrics are where traffic
// lives. What is here changes when the machine's network does (a DHCP lease,
// a cable pulled, a new adapter), which is exactly when the fleet should hear
// about it.
//
// Both CheckSystem modules gather through their own OS (sysfs and getifaddrs
// on unix, GetAdaptersAddresses on Windows) and publish through this one
// function, so an interface means the same thing on every host - the same
// rule host_facts follows for the `os` set. The per-platform spellings that
// have to agree (a MAC, a link state, an IPv6 link-local address) are
// normalized here, where both builds test them.
namespace network_facts {

// The fact set CheckSystem produces, and the one list in it. The enableable
// id is the dotted path, `network.interfaces`: that is the settings key.
extern const char *const set_network;
extern const char *const key_interfaces;
extern const char *const id_interfaces;

// One interface, as a platform's gather reads it. Empty strings, a zero speed
// and an empty address list mean "not known" and are omitted from the record.
struct interface_record {
  // The record id: the `name` keyword check_network reports for the same
  // interface. On unix the kernel's name (`eth0`, `ens192`); on Windows the
  // adapter's description (`Intel(R) Ethernet Connection I219-V`), which is
  // what Win32_NetworkAdapter - and so check_network - calls it.
  std::string id;
  // The name an operator gave the connection, where the platform has one
  // (Windows: `Ethernet`, `Wi-Fi`). check_network's net_connection_id.
  std::string display_name;
  std::string mac;     // see normalize_mac()
  std::string status;  // RFC 2863 operational state, see the vocabulary below
  long long speed_bps = 0;
  std::vector<std::string> addresses;  // IPv4 and IPv6, see normalize_address()
};

// Add the `network` set, with every interface in `interfaces` as a record of
// its `interfaces` list, to `out`. `taken_at` stamps when they were read.
void publish(const std::vector<interface_record> &interfaces, std::time_t taken_at, nscapi::facts::response &out);

// A hardware address as six lowercase, colon-separated octets
// (`00:1a:2b:3c:4d:5e`). Windows spells it `00-1A-2B-3C-4D-5E` and unix
// `00:1a:2b:3c:4d:5e`; one fleet, one spelling. An address that is all zeros
// (a tunnel, a loopback) is not an address and yields an empty string, as does
// anything that is not six hex octets.
std::string normalize_mac(const std::string &raw);

// The same, from the raw bytes GetAdaptersAddresses hands out.
std::string mac_from_bytes(const unsigned char *bytes, std::size_t length);

// An IP address as a string, without the `%zone` a platform appends to an
// IPv6 link-local address (`fe80::1%12` on Windows, `fe80::1%eth0` on unix).
// The zone is the interface's own index or name, which the record already
// says, and it is not the same string on two platforms.
std::string normalize_address(const std::string &raw);

// The link state vocabulary is RFC 2863's ifOperStatus, which both platforms
// already speak: `up`, `down`, `testing`, `unknown`, `dormant`, `notpresent`,
// `lowerlayerdown`. Linux writes it into /sys/class/net/<if>/operstate as
// those words; Windows hands out the numbers, and this maps them. An out of
// range number is `unknown`.
std::string status_from_oper_status(int oper_status);

// Read this host's interfaces. Implemented per platform, by each CheckSystem
// module. The loopback interface is left out: every host has one, it never
// changes, and it is not what anyone means by "the host's network".
std::vector<interface_record> gather();

}  // namespace network_facts
