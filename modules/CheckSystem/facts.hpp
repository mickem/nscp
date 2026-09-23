// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <ctime>
#include <string>
#include <vector>

// The data sources behind this module's fact sets that no existing check
// already exposes as a gatherer.
//
// `identity` and `hardware` reuse `hostname_check::gather_identity()` and
// `hardware_check::gather_hardware()` as they are; `os` is assembled from the
// same Win32 calls check_os_version makes; only the interface list needs a
// source of its own, because the collector's network rows carry throughput and
// no addresses, and inventory wants the addresses and none of the throughput.
//
// The field names mirror the Unix module's (modules/CheckSystemUnix/facts.h):
// `os`, `identity`, `hardware` and `network.interfaces` mean the same thing on
// both platforms, and a fleet query over them must not have to know which OS
// answered.
namespace check_system_facts {

struct os_facts {
  std::string family;   // always "windows" here
  std::string name;     // e.g. "Windows Server 2022"
  std::string version;  // major.minor.build, e.g. "10.0.20348"
  std::string kernel;   // major.minor.build.ubr
  std::string arch;     // e.g. "x86_64"
  std::time_t boot_time = 0;
  bool pending_reboot = false;
  std::string bios_version;
  std::string bios_manufacturer;
};

struct interface_facts {
  std::string id;  // the adapter's connection name - the check's instance name
  std::string mac;
  std::vector<std::string> addresses;
  long long speed_bps = 0;
  std::string state;  // "up", "down", "unknown"
};

// Assembled from GetVersionEx/GetNativeSystemInfo, the UBR registry value and
// the pending-reboot signals, plus WMI for the BIOS fields (best-effort, as
// check_os_version does it). `error` is set only when the version itself could
// not be read.
os_facts gather_os(std::string &error);

// GetAdaptersAddresses: one record per adapter, with its addresses. `error` is
// set when the enumeration failed, which the core reports on the fact set
// rather than blanking the inventory.
std::vector<interface_facts> gather_interfaces(std::string &error);

// Map an IF_OPER_STATUS to the `state` vocabulary the Unix module uses
// (`up`, `down`, `unknown`) - the same word must mean the same thing on both
// platforms or a fleet query over it is meaningless. Pure and unit-testable.
std::string operational_status_to_state(unsigned long status);

// Format a MAC address from its raw bytes as `00:50:56:aa:bb:cc`, which is how
// the Unix module reports one. Pure and unit-testable.
std::string format_mac(const unsigned char *address, unsigned long length);

// Format an address from its raw bytes, exactly as inet_ntop does on the Unix
// side: dotted quad for IPv4, RFC 5952 for IPv6 (lower case hex, the longest
// run of zero groups compressed once, an embedded IPv4 address written as
// one). Written here rather than called from the platform because InetNtop is
// Vista-or-later and this agent still builds for XP - and because a pure
// formatter is testable on any platform, which the API is not.
std::string format_ipv4(const unsigned char address[4]);
std::string format_ipv6(const unsigned char address[16]);

}  // namespace check_system_facts
