// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Windows.h must precede dsrole.h; the capital W keeps clang-format's
// case-sensitive include sort from breaking that order.
#include <Windows.h>
#include <dsrole.h>

#include <memory>

namespace check_ad {

// DsRoleGetPrimaryDomainInformation has its own allocator (not NetApiBuffer).
struct ds_role_deleter {
  void operator()(void *buffer) const noexcept {
    if (buffer != nullptr) DsRoleFreeMemory(buffer);
  }
};
typedef std::unique_ptr<DSROLE_PRIMARY_DOMAIN_INFO_BASIC, ds_role_deleter> ds_role_ptr;

// The basic role/domain information of `server` (null = this machine), or an
// empty pointer when the lookup fails.
inline ds_role_ptr primary_domain_info(const wchar_t *server) {
  PBYTE raw = nullptr;
  if (DsRoleGetPrimaryDomainInformation(server, DsRolePrimaryDomainInfoBasic, &raw) != ERROR_SUCCESS) return ds_role_ptr();
  return ds_role_ptr(reinterpret_cast<DSROLE_PRIMARY_DOMAIN_INFO_BASIC *>(raw));
}

}  // namespace check_ad
