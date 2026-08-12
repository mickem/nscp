// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ad_replication_source.hpp"

// Windows.h must precede ntdsapi.h; the capital W keeps clang-format's
// case-sensitive include sort from breaking that order.
#include <Windows.h>
#include <ntdsapi.h>

#include <error/error.hpp>
#include <str/utf8.hpp>
#include <vector>

namespace ad_replication_source {

namespace {

std::string local_dns_hostname() {
  DWORD size = 0;
  GetComputerNameExW(ComputerNameDnsHostname, nullptr, &size);
  if (size == 0) return "";
  std::vector<wchar_t> buf(size + 1, L'\0');
  if (!GetComputerNameExW(ComputerNameDnsHostname, buf.data(), &size)) return "";
  return utf8::cvt<std::string>(std::wstring(buf.data()));
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

bool fetch(const std::string &server, std::vector<ad_replication_filter::filter_obj_ptr> &out, std::string &error, bool &not_a_dc) {
  not_a_dc = false;
  // DsBind with a NULL server binds to *some* DC in the domain, not this host,
  // so always name the target explicitly: replication state is per-DC.
  const std::string target = server.empty() ? local_dns_hostname() : server;
  if (target.empty()) {
    error = "Failed to resolve the local computer name";
    return false;
  }
  const std::wstring target_w = utf8::cvt<std::wstring>(target);

  HANDLE hds = nullptr;
  DWORD rc = DsBindW(target_w.c_str(), nullptr, &hds);
  if (rc != ERROR_SUCCESS) {
    // The directory service not being reachable on the target is what a bind
    // failure means in practice: a non-DC (or a stopped NTDS). Report it as
    // the not-a-DC contract rather than a replication failure.
    not_a_dc = true;
    error = "Failed to bind to the directory service on " + target + ": " + error::lookup::last_error(rc);
    return false;
  }

  DS_REPL_NEIGHBORSW *neighbors = nullptr;
  rc = DsReplicaGetInfoW(hds, DS_REPL_INFO_NEIGHBORS, nullptr, nullptr, reinterpret_cast<VOID **>(&neighbors));
  if (rc != ERROR_SUCCESS) {
    error = "Failed to read replication state from " + target + ": " + error::lookup::last_error(rc);
    DsUnBindW(&hds);
    return false;
  }

  for (DWORD i = 0; i < neighbors->cNumNeighbors; ++i) {
    const DS_REPL_NEIGHBORW &n = neighbors->rgNeighbor[i];
    ad_replication_filter::filter_obj_ptr obj(new ad_replication_filter::filter_obj());
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
    if (n.dwLastSyncResult != ERROR_SUCCESS) obj->last_error_message = error::lookup::last_error(n.dwLastSyncResult);
    out.push_back(obj);
  }

  DsReplicaFreeInfo(DS_REPL_INFO_NEIGHBORS, neighbors);
  DsUnBindW(&hds);
  return true;
}

}  // namespace ad_replication_source
