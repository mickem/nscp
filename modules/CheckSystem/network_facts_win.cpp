// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// network_facts::gather() for Windows: GetAdaptersAddresses, which answers
// from the IP helper's own tables - no WMI, so no COM apartment and none of
// the provider stalls check_network has to run on its collector thread.

// Winsock before Windows.h, or Windows.h drags in the old winsock.h and the
// two collide.
#include <winsock2.h>
#include <ws2tcpip.h>
// Windows.h before iphlpapi.h; the capital W keeps clang-format's
// case-sensitive include sort from reordering them.
#include <Windows.h>
#include <iphlpapi.h>

#include <algorithm>
#include <facts/network_facts.hpp>
#include <stdexcept>
#include <str/utf8.hpp>
#include <string>
#include <vector>

namespace {

// Winsock has to be started for getnameinfo, even for a numeric host. It is
// reference counted, so starting it here next to whatever else in the process
// already has is harmless, and scoping it keeps this function self-contained.
class winsock_scope {
 public:
  winsock_scope() {
    WSADATA data;
    started_ = WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }
  ~winsock_scope() {
    if (started_) WSACleanup();
  }
  bool started() const { return started_; }

 private:
  bool started_;
};

std::string to_utf8(const wchar_t *text) { return text == nullptr ? "" : utf8::cvt<std::string>(std::wstring(text)); }

std::vector<std::string> read_addresses(const IP_ADAPTER_ADDRESSES *adapter, const bool can_format) {
  std::vector<std::string> addresses;
  if (!can_format) return addresses;
  for (const IP_ADAPTER_UNICAST_ADDRESS *address = adapter->FirstUnicastAddress; address != nullptr; address = address->Next) {
    const SOCKET_ADDRESS &socket_address = address->Address;
    if (socket_address.lpSockaddr == nullptr) continue;
    const int family = socket_address.lpSockaddr->sa_family;
    if (family != AF_INET && family != AF_INET6) continue;
    char host[NI_MAXHOST] = {0};
    if (getnameinfo(socket_address.lpSockaddr, socket_address.iSockaddrLength, host, sizeof(host), nullptr, 0, NI_NUMERICHOST) != 0) continue;
    addresses.push_back(network_facts::normalize_address(host));
  }
  // Sorted, so the order the stack happens to list them in is not a change.
  std::sort(addresses.begin(), addresses.end());
  addresses.erase(std::unique(addresses.begin(), addresses.end()), addresses.end());
  return addresses;
}

}  // namespace

std::vector<network_facts::interface_record> network_facts::gather() {
  std::vector<interface_record> interfaces;
  const winsock_scope winsock;

  const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
  ULONG size = 16 * 1024;
  std::vector<unsigned char> buffer;
  ULONG status = ERROR_BUFFER_OVERFLOW;
  // The table can grow between the sizing call and the read (an adapter
  // arriving), so retry a few times rather than trusting one answer.
  for (int attempt = 0; attempt < 4 && status == ERROR_BUFFER_OVERFLOW; ++attempt) {
    buffer.resize(size);
    status = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()), &size);
  }
  if (status == ERROR_NO_DATA) return interfaces;
  if (status != NO_ERROR) throw std::runtime_error("GetAdaptersAddresses failed: " + std::to_string(status));

  for (const IP_ADAPTER_ADDRESSES *adapter = reinterpret_cast<const IP_ADAPTER_ADDRESSES *>(buffer.data()); adapter != nullptr; adapter = adapter->Next) {
    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
    interface_record nic;
    // The description is what Win32_NetworkAdapter calls Name, and so what
    // check_network reports as `name`: the record id has to be that value.
    nic.id = to_utf8(adapter->Description);
    if (nic.id.empty()) nic.id = adapter->AdapterName == nullptr ? "" : adapter->AdapterName;
    nic.display_name = to_utf8(adapter->FriendlyName);
    nic.mac = mac_from_bytes(adapter->PhysicalAddress, adapter->PhysicalAddressLength);
    nic.status = status_from_oper_status(static_cast<int>(adapter->OperStatus));
#if _WIN32_WINNT >= 0x0600
    // Vista added the link speed to this structure. The XP build compiles
    // against the older layout and reports no speed, rather than reading past
    // the end of what XP fills in. All ones is the API's "unknown".
    if (adapter->TransmitLinkSpeed > 0 && adapter->TransmitLinkSpeed != static_cast<ULONG64>(-1)) {
      nic.speed_bps = static_cast<long long>(adapter->TransmitLinkSpeed);
    }
#endif
    nic.addresses = read_addresses(adapter, winsock.started());
    interfaces.push_back(nic);
  }
  // The stack lists adapters in its own order: sorted by id so the same
  // network is the same document.
  std::sort(interfaces.begin(), interfaces.end(), [](const interface_record &a, const interface_record &b) { return a.id < b.id; });
  return interfaces;
}
