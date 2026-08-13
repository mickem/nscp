// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ad_replication_source.hpp"

// boost/asio must precede Windows.h so winsock2.h is included first.
#include <boost/asio.hpp>

// Windows.h must precede ntdsapi.h/dsrole.h; the capital W keeps
// clang-format's case-sensitive include sort from breaking that order. asio
// defines WIN32_LEAN_AND_MEAN, which drops rpc.h from Windows.h, so pull it in
// explicitly: ntdsapi.h needs RPC_AUTH_IDENTITY_HANDLE.
#include <Windows.h>
#include <rpc.h>

#include <dsrole.h>
#include <ntdsapi.h>

#include <chrono>
#include <memory>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <vector>

#include "win32_error.hpp"

namespace ad_replication_source {

namespace {

// --- RAII wrappers over the directory-service handles -----------------------

// DsBindW hands out a binding that DsUnBindW must take back (by address).
class ds_binding {
 public:
  ds_binding() = default;
  ~ds_binding() {
    if (handle_ != nullptr) DsUnBindW(&handle_);
  }
  ds_binding(const ds_binding &) = delete;
  ds_binding &operator=(const ds_binding &) = delete;

  DWORD bind(const std::wstring &server) { return DsBindW(server.c_str(), nullptr, &handle_); }
  HANDLE get() const { return handle_; }

 private:
  HANDLE handle_ = nullptr;
};

// DsReplicaGetInfoW allocates a result that DsReplicaFreeInfo releases, and
// the release needs the same info type the fetch was made with - so the two
// belong together rather than in a bare pointer plus a matching free call.
class ds_replica_info {
 public:
  explicit ds_replica_info(DS_REPL_INFO_TYPE type) : type_(type) {}
  ~ds_replica_info() {
    if (data_ != nullptr) DsReplicaFreeInfo(type_, data_);
  }
  ds_replica_info(const ds_replica_info &) = delete;
  ds_replica_info &operator=(const ds_replica_info &) = delete;

  DWORD fetch(HANDLE binding) { return DsReplicaGetInfoW(binding, type_, nullptr, nullptr, &data_); }
  const DS_REPL_NEIGHBORSW *neighbors() const { return static_cast<const DS_REPL_NEIGHBORSW *>(data_); }

 private:
  DS_REPL_INFO_TYPE type_;
  VOID *data_ = nullptr;
};

// DsRoleGetPrimaryDomainInformation has its own allocator (not NetApiBuffer).
struct ds_role_deleter {
  void operator()(void *buffer) const noexcept {
    if (buffer != nullptr) DsRoleFreeMemory(buffer);
  }
};
typedef std::unique_ptr<DSROLE_PRIMARY_DOMAIN_INFO_BASIC, ds_role_deleter> ds_role_ptr;

// --- helpers ----------------------------------------------------------------

std::string local_dns_hostname() {
  DWORD size = 0;
  GetComputerNameExW(ComputerNameDnsHostname, nullptr, &size);
  if (size == 0) return "";
  std::wstring buf(size, L'\0');
  if (!GetComputerNameExW(ComputerNameDnsHostname, &buf[0], &size)) return "";
  buf.resize(size);
  return utf8::cvt<std::string>(buf);
}

// Is the target actually a domain controller? A failed DsBind says nothing
// about that on its own: a member server and a DC whose NTDS service has
// stopped both refuse the bind the same way. Asking the machine role first is
// what separates "there is nothing here to check" (benign, deploy fleet-wide)
// from "this DC is not answering" - the very outage this check exists for.
// Returns false only when the role is known and is not a DC; an unavailable
// role lookup leaves the benign reading in place rather than raising a false
// alarm.
bool looks_like_a_domain_controller(const std::string &server) {
  const std::wstring server_w = utf8::cvt<std::wstring>(server);
  PBYTE raw = nullptr;
  if (DsRoleGetPrimaryDomainInformation(server.empty() ? nullptr : server_w.c_str(), DsRolePrimaryDomainInfoBasic, &raw) != ERROR_SUCCESS) return true;
  const ds_role_ptr info(reinterpret_cast<DSROLE_PRIMARY_DOMAIN_INFO_BASIC *>(raw));
  if (!info) return true;
  return info->MachineRole == DsRole_RoleBackupDomainController || info->MachineRole == DsRole_RolePrimaryDomainController;
}

// DsBindW itself has no timeout: against a black-holed host it blocks for the
// full TCP retransmit window (~21s+, possibly per RPC endpoint), blowing the
// transport's command timeout. Before binding to an explicitly named remote
// server, require its RPC endpoint mapper (port 135) to answer a TCP connect
// within a bounded deadline so an unreachable DC fails fast instead.
bool can_reach_rpc(const std::string &host, int timeout_ms, std::string &error) {
  namespace asio = boost::asio;
  using boost::asio::ip::tcp;

  asio::io_context io;
  tcp::resolver resolver(io);
  tcp::socket socket(io);
  bool connected = false;

  resolver.async_resolve(host, "135", [&](const boost::system::error_code &ec, tcp::resolver::results_type results) {
    if (ec) {
      error = "resolve failed: " + ec.message();
      return;
    }
    asio::async_connect(socket, results, [&](const boost::system::error_code &ec, const tcp::endpoint &) {
      if (ec)
        error = "connect failed: " + ec.message();
      else
        connected = true;
    });
  });

  io.run_for(std::chrono::milliseconds(timeout_ms));
  if (!connected && error.empty()) error = "no answer within " + str::xtos(timeout_ms) + "ms";
  boost::system::error_code ignored;
  socket.close(ignored);
  return connected;
}

long long filetime_to_epoch(const FILETIME &ft) {
  const unsigned long long ticks = (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
  if (ticks == 0) return 0;
  // FILETIME epoch (1601) to unix epoch (1970) in 100ns ticks.
  const unsigned long long kEpochDelta = 116444736000000000ULL;
  if (ticks < kEpochDelta) return 0;
  return static_cast<long long>((ticks - kEpochDelta) / 10000000ULL);
}

}  // namespace

bool fetch(const std::string &server, int timeout_ms, std::vector<ad_replication_filter::filter_obj_ptr> &out, std::string &error, bool &not_a_dc) {
  not_a_dc = false;
  // DsBind with a NULL server binds to *some* DC in the domain, not this host,
  // so always name the target explicitly: replication state is per-DC.
  const std::string target = server.empty() ? local_dns_hostname() : server;
  if (target.empty()) {
    error = "Failed to resolve the local computer name";
    return false;
  }

  if (!server.empty()) {
    std::string reach_error;
    if (!can_reach_rpc(server, timeout_ms, reach_error)) {
      error = "Cannot reach the RPC endpoint mapper on " + server + " (port 135): " + reach_error;
      return false;
    }
  }

  ds_binding binding;
  DWORD rc = binding.bind(utf8::cvt<std::wstring>(target));
  if (rc != ERROR_SUCCESS) {
    // Only a machine that is genuinely not a domain controller gets the benign
    // contract. A DC that fails to bind (stopped NTDS, access denied, RPC
    // unavailable) is a real failure and must not be filed as "nothing here".
    not_a_dc = !looks_like_a_domain_controller(server);
    error = "Failed to bind to the directory service on " + target + ": " + check_ad::win32_error(rc);
    return false;
  }

  ds_replica_info info(DS_REPL_INFO_NEIGHBORS);
  rc = info.fetch(binding.get());
  if (rc != ERROR_SUCCESS) {
    error = "Failed to read replication state from " + target + ": " + check_ad::win32_error(rc);
    return false;
  }
  const DS_REPL_NEIGHBORSW *neighbors = info.neighbors();
  if (neighbors == nullptr) {
    error = "The directory service on " + target + " returned no replication state";
    return false;
  }

  for (DWORD i = 0; i < neighbors->cNumNeighbors; ++i) {
    const DS_REPL_NEIGHBORW &n = neighbors->rgNeighbor[i];
    ad_replication_filter::filter_obj_ptr obj = std::make_shared<ad_replication_filter::filter_obj>();
    if (n.pszNamingContext != nullptr) obj->naming_context = utf8::cvt<std::string>(std::wstring(n.pszNamingContext));
    if (n.pszSourceDsaDN != nullptr) {
      obj->source_dsa_dn = utf8::cvt<std::string>(std::wstring(n.pszSourceDsaDN));
      obj->source_server = ad_replication_filter::extract_server_from_ntds_dn(obj->source_dsa_dn);
    }
    if (n.pszSourceDsaAddress != nullptr) obj->source_address = utf8::cvt<std::string>(std::wstring(n.pszSourceDsaAddress));
    obj->last_attempt = filetime_to_epoch(n.ftimeLastSyncAttempt);
    obj->last_success = filetime_to_epoch(n.ftimeLastSyncSuccess);
    obj->last_error = n.dwLastSyncResult;
    obj->consecutive_failures = n.cNumConsecutiveSyncFailures;
    if (n.dwLastSyncResult != ERROR_SUCCESS) obj->last_error_message = check_ad::win32_error(n.dwLastSyncResult);
    out.push_back(obj);
  }

  return true;
}

}  // namespace ad_replication_source
