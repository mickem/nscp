// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "dhcp_source.hpp"

// Windows.h must precede dhcpsapi.h; the capital W keeps clang-format's
// case-sensitive include sort from breaking that order.
#include <Windows.h>
#include <dhcpsapi.h>

#include <boost/algorithm/string/trim.hpp>
#include <error/error.hpp>
#include <str/utf8.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_object_gather.hpp>

// dhcpsapi.h declares the DHCP error codes; keep the one used here even where
// an older SDK leaves it out.
#ifndef ERROR_DHCP_SUBNET_NOT_PRESENT
#define ERROR_DHCP_SUBNET_NOT_PRESENT 20005L
#endif

namespace dhcp_source {

namespace {

// The local DHCP server. The management API binds to it over local RPC.
const LPCWSTR kLocalServer = nullptr;

const char *const kCounterObject = "DHCP Server";

// FormatMessage ends its text in CRLF; a check result is one line.
std::string win32_error(const DWORD code) { return boost::algorithm::trim_copy(error::lookup::last_error(static_cast<unsigned long>(code))); }

// What a failed management call means for the operator. A host without the
// role (or with the service stopped) has no RPC endpoint to bind to; that is
// the common "deployed fleet-wide" case and gets a message that says so.
std::string describe_failure(const std::string &what, const DWORD rc) {
  switch (rc) {
    case RPC_S_SERVER_UNAVAILABLE:
    case EPT_S_NOT_REGISTERED:
    case RPC_S_UNKNOWN_IF:
    case ERROR_SERVICE_DOES_NOT_EXIST:
      return "DHCP Server service is not running or not installed on this host: " + win32_error(rc);
    case ERROR_ACCESS_DENIED:
      return what + ": access denied - the service account must be a member of DHCP Users or DHCP Administrators";
    default:
      return what + ": " + win32_error(rc);
  }
}

std::string wide_to_utf8(const LPWSTR s) { return s == nullptr ? std::string() : utf8::cvt<std::string>(std::wstring(s)); }

void rpc_free(void *p) {
  if (p != nullptr) DhcpRpcFreeMemory(p);
}

// Every pointer in an RPC-returned structure is a separate allocation, so each
// one is released on its own before the structure itself.
struct ip_array_holder {
  LPDHCP_IP_ARRAY p = nullptr;
  ip_array_holder() = default;
  ip_array_holder(const ip_array_holder &) = delete;
  ip_array_holder &operator=(const ip_array_holder &) = delete;
  ~ip_array_holder() {
    if (p == nullptr) return;
    rpc_free(p->Elements);
    rpc_free(p);
  }
};

struct subnet_info_holder {
  LPDHCP_SUBNET_INFO p = nullptr;
  subnet_info_holder() = default;
  subnet_info_holder(const subnet_info_holder &) = delete;
  subnet_info_holder &operator=(const subnet_info_holder &) = delete;
  ~subnet_info_holder() {
    if (p == nullptr) return;
    rpc_free(p->SubnetName);
    rpc_free(p->SubnetComment);
    rpc_free(p->PrimaryHost.NetBiosName);
    rpc_free(p->PrimaryHost.HostName);
    rpc_free(p);
  }
};

struct mib_info_holder {
  LPDHCP_MIB_INFO p = nullptr;
  mib_info_holder() = default;
  mib_info_holder(const mib_info_holder &) = delete;
  mib_info_holder &operator=(const mib_info_holder &) = delete;
  ~mib_info_holder() {
    if (p == nullptr) return;
    rpc_free(p->ScopeInfo);
    rpc_free(p);
  }
};

// dhcpsapi.dll is delay-loaded (see CMakeLists.txt): nothing maps it until
// the first DHCP call, and if the helper cannot find it then, it raises a
// structured exception rather than returning an error. So map it here first,
// by absolute System32 path (neither the working directory nor PATH may
// supply it), and report a missing DLL as a plain failure. Once loaded, the
// helper resolves its imports against this module instead of searching for
// it. The module stays mapped for the life of the process, as a delay-loaded
// DLL does anyway.
struct dhcpsapi_load {
  HMODULE module;
  DWORD error;
};

dhcpsapi_load load_dhcpsapi() {
  wchar_t directory[MAX_PATH + 1] = {};
  const UINT length = GetSystemDirectoryW(directory, MAX_PATH + 1);
  if (length == 0 || length > MAX_PATH) return {nullptr, GetLastError()};
  const HMODULE module = LoadLibraryW((std::wstring(directory) + L"\\dhcpsapi.dll").c_str());
  return {module, module == nullptr ? GetLastError() : static_cast<DWORD>(ERROR_SUCCESS)};
}

bool ensure_dhcpsapi(std::string &error) {
  static const dhcpsapi_load loaded = load_dhcpsapi();
  if (loaded.module != nullptr) return true;
  error = "The DHCP Server Management API (dhcpsapi.dll) is not available on this host: " + win32_error(loaded.error);
  return false;
}

bool enum_subnets(std::vector<DHCP_IP_ADDRESS> &subnets, std::string &error) {
  DHCP_RESUME_HANDLE resume = 0;
  for (;;) {
    ip_array_holder batch;
    DWORD read = 0, total = 0;
    const DWORD rc = DhcpEnumSubnets(kLocalServer, &resume, 65536, &batch.p, &read, &total);
    if (rc == ERROR_NO_MORE_ITEMS) return true;
    if (rc != ERROR_SUCCESS && rc != ERROR_MORE_DATA) {
      error = describe_failure("Failed to list the DHCP scopes", rc);
      return false;
    }
    if (batch.p != nullptr && batch.p->Elements != nullptr) {
      for (DWORD i = 0; i < batch.p->NumElements; ++i) subnets.push_back(batch.p->Elements[i]);
    }
    // ERROR_SUCCESS is the last batch. A "more data" that delivered nothing
    // would never advance the resume handle, so it ends the walk too.
    if (rc == ERROR_SUCCESS || read == 0) return true;
  }
}

bool read_scope_stats(std::map<DHCP_IP_ADDRESS, dhcp_filter::scope_stats> &stats, std::string &error) {
  mib_info_holder mib;
  const DWORD rc = DhcpGetMibInfo(kLocalServer, &mib.p);
  if (rc != ERROR_SUCCESS) {
    error = describe_failure("Failed to read the DHCP scope statistics", rc);
    return false;
  }
  if (mib.p == nullptr) {
    error = "The DHCP server returned no scope statistics";
    return false;
  }
  if (mib.p->ScopeInfo == nullptr) return true;
  for (DWORD i = 0; i < mib.p->Scopes; ++i) {
    const SCOPE_MIB_INFO &s = mib.p->ScopeInfo[i];
    dhcp_filter::scope_stats &entry = stats[s.Subnet];
    entry.in_use = s.NumAddressesInuse;
    entry.available = s.NumAddressesFree;
    entry.pending_offers = s.NumPendingOffers;
  }
  return true;
}

}  // namespace

bool fetch_scopes(std::vector<dhcp_filter::scope_obj_ptr> &out, std::string &error) {
  if (!ensure_dhcpsapi(error)) return false;
  std::vector<DHCP_IP_ADDRESS> subnets;
  if (!enum_subnets(subnets, error)) return false;

  // Statistics are read after the list: a scope created in between has no
  // entry and is reported with stats_known false; one deleted in between
  // fails its DhcpGetSubnetInfo below and is skipped.
  std::map<DHCP_IP_ADDRESS, dhcp_filter::scope_stats> stats;
  if (!read_scope_stats(stats, error)) return false;

  for (const DHCP_IP_ADDRESS subnet : subnets) {
    subnet_info_holder info;
    const DWORD rc = DhcpGetSubnetInfo(kLocalServer, subnet, &info.p);
    if (rc == ERROR_DHCP_SUBNET_NOT_PRESENT) continue;
    if (rc != ERROR_SUCCESS) {
      error = describe_failure("Failed to read DHCP scope " + dhcp_filter::format_ipv4(subnet), rc);
      return false;
    }
    if (info.p == nullptr) {
      error = "The DHCP server returned no information for scope " + dhcp_filter::format_ipv4(subnet);
      return false;
    }
    const auto it = stats.find(subnet);
    out.push_back(dhcp_filter::make_scope(subnet, info.p->SubnetMask, wide_to_utf8(info.p->SubnetName), wide_to_utf8(info.p->SubnetComment),
                                          static_cast<int>(info.p->SubnetState), it == stats.end() ? nullptr : &it->second));
  }
  return true;
}

bool fetch_counters(std::map<std::string, double> &out, std::string &error) {
  std::vector<std::string> counters;
  for (std::size_t i = 0; i < dhcp_filter::kServerCounterCount; ++i) counters.push_back(dhcp_filter::kServerCounters[i].counter);
  try {
    out = PDH::gather_object_values(kCounterObject, counters, true);
    return true;
  } catch (const PDH::pdh_exception &e) {
    std::string reason = e.reason();
    boost::algorithm::trim(reason);
    error = "DHCP Server performance counters are not available - is the DHCP Server role installed on this host? (" + reason + ")";
    return false;
  }
}

}  // namespace dhcp_source
