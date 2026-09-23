// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "facts.hpp"

// clang-format off
#ifdef WIN32
// winsock2.h before windows.h, or windows.h pulls in the winsock 1 headers
// and the two conflict - the ordering check_connections.cpp uses for the same
// reason.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#endif
// clang-format on

#include <cstdio>
#include <string>
#include <vector>

#ifdef WIN32
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <win/com_helpers.hpp>
#include <win/registry.hpp>
#include <win/sysinfo/win_sysinfo.hpp>
#include <win/wmi/wmi_query.hpp>

#include "check_pending_reboot.hpp"
#include "filter.hpp"
#include "tick_count.h"
#endif

namespace check_system_facts {

std::string format_mac(const unsigned char *address, const unsigned long length) {
  if (address == nullptr || length == 0) return std::string();
  std::string out;
  out.reserve(length * 3);
  for (unsigned long i = 0; i < length; ++i) {
    char pair[4];
    // Lower case with colons, which is what /sys/class/net/<iface>/address
    // gives on Linux: one spelling of a MAC across the fleet or a server-side
    // join on it finds nothing.
    std::snprintf(pair, sizeof(pair), "%02x", static_cast<unsigned int>(address[i]));
    if (i > 0) out += ':';
    out += pair;
  }
  return out;
}

std::string format_ipv4(const unsigned char address[4]) {
  char text[16];
  std::snprintf(text, sizeof(text), "%u.%u.%u.%u", static_cast<unsigned int>(address[0]), static_cast<unsigned int>(address[1]),
                static_cast<unsigned int>(address[2]), static_cast<unsigned int>(address[3]));
  return text;
}

std::string format_ipv6(const unsigned char address[16]) {
  unsigned int groups[8];
  for (int i = 0; i < 8; ++i) {
    groups[i] = (static_cast<unsigned int>(address[i * 2]) << 8) | static_cast<unsigned int>(address[i * 2 + 1]);
  }

  // RFC 5952: compress the longest run of two or more zero groups, leftmost on
  // a tie. Without one rule two agents would report the same address as two
  // different strings and the server could not join them.
  int best_start = -1;
  int best_length = 0;
  int run_start = -1;
  int run_length = 0;
  for (int i = 0; i < 8; ++i) {
    if (groups[i] == 0) {
      if (run_start < 0) run_start = i;
      ++run_length;
      if (run_length > best_length) {
        best_start = run_start;
        best_length = run_length;
      }
    } else {
      run_start = -1;
      run_length = 0;
    }
  }
  if (best_length < 2) best_start = -1;

  // An IPv4-mapped, IPv4-compatible or IPv4-translated address has its last 32
  // bits written as a dotted quad. The three shapes are exactly the ones
  // inet_ntop treats that way, so the two platforms agree character for
  // character; ::1 is deliberately not one of them.
  const bool embedded_v4 =
      best_start == 0 && (best_length == 6 || (best_length == 7 && groups[7] != 0x0001) || (best_length == 5 && groups[5] == 0xffff));

  std::string out;
  int i = 0;
  while (i < 8) {
    if (i == best_start) {
      out += "::";
      i += best_length;
      continue;
    }
    if (!out.empty() && out[out.size() - 1] != ':') out += ':';
    if (i == 6 && embedded_v4) {
      out += format_ipv4(address + 12);
      break;
    }
    char group[8];
    std::snprintf(group, sizeof(group), "%x", groups[i]);
    out += group;
    ++i;
  }
  return out;
}

std::string operational_status_to_state(const unsigned long status) {
  // IF_OPER_STATUS (RFC 2863): 1 up, 2 down, 3 testing, 4 unknown, 5 dormant,
  // 6 not present, 7 lower layer down. Anything that is not plainly up or down
  // is `unknown` rather than a word Linux never produces.
  switch (status) {
    case 1:
      return "up";
    case 2:
    case 6:
    case 7:
      return "down";
    default:
      return "unknown";
  }
}

#ifdef WIN32

namespace {

// The BIOS fields, best-effort exactly as check_os_version fetches them: a
// stripped-down VM without the WMI class leaves them empty rather than failing
// the whole set.
void fetch_bios(os_facts &out) {
  // Scoped COM init, tolerating an apartment already initialised elsewhere -
  // the same preamble check_os_version's BIOS fetch uses.
  const com_helper::mta_scope com;
  try {
    wmi_impl::query query("SELECT SMBIOSBIOSVersion, Manufacturer FROM Win32_BIOS", "root\\CIMV2", "", "");
    wmi_impl::row_enumerator rows = query.execute();
    if (rows.has_next()) {
      const wmi_impl::row r = rows.get_next();
      out.bios_version = r.get_string("SMBIOSBIOSVersion");
      out.bios_manufacturer = r.get_string("Manufacturer");
    }
  } catch (...) {
    // Inventory only.
  }
}

}  // namespace

os_facts gather_os(std::string &error) {
  os_facts found;
  found.family = "windows";

  OSVERSIONINFOEX *info = windows::system_info::get_versioninfo();
  if (info == nullptr) {
    error = "could not read the Windows version";
    return found;
  }
  found.name = windows::system_info::get_version_string();
  found.version = str::xtos(static_cast<long long>(info->dwMajorVersion)) + "." + str::xtos(static_cast<long long>(info->dwMinorVersion)) + "." +
                  str::xtos(static_cast<long long>(info->dwBuildNumber));

  // GetNativeSystemInfo reports the true hardware architecture even for a
  // 32-bit process under WOW64, and the same mapping check_os_version uses, so
  // `os.arch` and the check's `arch` keyword are the same string.
  SYSTEM_INFO sysinfo = {};
  GetNativeSystemInfo(&sysinfo);
  found.arch = os_version_filter::arch_from_native(sysinfo.wProcessorArchitecture);

  // The UBR (the ".xxxx" in 10.0.19045.3803) lives only in the registry; the
  // 64-bit view so a 32-bit agent under WOW64 reads the native key.
  const win_registry::value_info ubr =
      win_registry::read_value(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "UBR", KEY_WOW64_64KEY);
  found.kernel = os_version_filter::format_kernel_version(info->dwMajorVersion, info->dwMinorVersion, info->dwBuildNumber,
                                                          ubr.exists ? ubr.int_value : 0);

  // Boot time derived from the tick count, the same source check_uptime uses,
  // so the two never disagree by a reboot.
  const ULONGLONG uptime_ms = nscpGetTickCount64();
  if (uptime_ms > 0) found.boot_time = ::time(nullptr) - static_cast<std::time_t>(uptime_ms / 1000ull);

  // A machine waiting for a reboot is inventory an operator acts on, and the
  // signals are a registry read the check already implements.
  try {
    found.pending_reboot = pending_reboot_check::gather_pending_reboot().any();
  } catch (...) {
    // Best-effort: an unreadable key leaves the flag false, as it does for the
    // check itself.
  }

  fetch_bios(found);
  return found;
}

std::vector<interface_facts> gather_interfaces(std::string &error) {
  std::vector<interface_facts> interfaces;

  // GetAdaptersAddresses wants a buffer it sizes itself; the documented dance
  // is to ask with a guess and grow once.
  ULONG size = 16 * 1024;
  std::vector<unsigned char> buffer(size);
  const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
  ULONG result = ::GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, reinterpret_cast<PIP_ADAPTER_ADDRESSES>(&buffer[0]), &size);
  if (result == ERROR_BUFFER_OVERFLOW) {
    buffer.resize(size);
    result = ::GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, reinterpret_cast<PIP_ADAPTER_ADDRESSES>(&buffer[0]), &size);
  }
  if (result != NO_ERROR) {
    error = "could not enumerate network adapters (GetAdaptersAddresses failed: " + str::xtos(static_cast<long long>(result)) + ")";
    return interfaces;
  }

  for (PIP_ADAPTER_ADDRESSES adapter = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(&buffer[0]); adapter != nullptr; adapter = adapter->Next) {
    interface_facts found;
    // The friendly connection name ("Ethernet 2"), which is what
    // check_network reports as its instance, so a failing check and this
    // record name the same adapter. The adapter's GUID would be stable too
    // but nobody, including the check, speaks it.
    found.id = adapter->FriendlyName != nullptr ? utf8::cvt<std::string>(std::wstring(adapter->FriendlyName)) : std::string();
    if (found.id.empty() && adapter->AdapterName != nullptr) found.id = adapter->AdapterName;
    if (found.id.empty()) continue;

    found.mac = format_mac(adapter->PhysicalAddress, adapter->PhysicalAddressLength);
    found.state = operational_status_to_state(adapter->OperStatus);
    // Windows reports receive and transmit speeds separately; the link speed
    // an operator means is the transmit one, and ~0ULL is the "unknown" a
    // virtual adapter reports.
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0600
    if (adapter->TransmitLinkSpeed != 0 && adapter->TransmitLinkSpeed != static_cast<ULONG64>(-1)) {
      found.speed_bps = static_cast<long long>(adapter->TransmitLinkSpeed);
    }
#endif

    for (PIP_ADAPTER_UNICAST_ADDRESS unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
      if (unicast->Address.lpSockaddr == nullptr) continue;
      if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
        const sockaddr_in *in = reinterpret_cast<const sockaddr_in *>(unicast->Address.lpSockaddr);
        found.addresses.push_back(format_ipv4(reinterpret_cast<const unsigned char *>(&in->sin_addr)));
      } else if (unicast->Address.lpSockaddr->sa_family == AF_INET6) {
        const sockaddr_in6 *in6 = reinterpret_cast<const sockaddr_in6 *>(unicast->Address.lpSockaddr);
        found.addresses.push_back(format_ipv6(reinterpret_cast<const unsigned char *>(&in6->sin6_addr)));
      }
    }
    interfaces.push_back(found);
  }
  return interfaces;
}

#else

// The module is Windows-only; these exist so the pure helpers above can be
// unit tested on any platform without pulling in the Win32 gatherers.
os_facts gather_os(std::string &error) {
  error = "facts are not available on this platform";
  return os_facts();
}

std::vector<interface_facts> gather_interfaces(std::string &error) {
  error = "facts are not available on this platform";
  return std::vector<interface_facts>();
}

#endif

}  // namespace check_system_facts
