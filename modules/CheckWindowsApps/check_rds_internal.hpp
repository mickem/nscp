// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>
#include <vector>

namespace check_rds {
namespace check_rds_internal {

// One Win32_TSLicenseKeyPack row, decoupled from WMI so the check logic and
// rendering can be unit tested without a licensing server.
struct license_key_pack {
  long long id = 0;
  std::string description;      // TypeAndModel, e.g. "RDS Per User CAL"
  std::string product_version;  // e.g. "Windows Server 2022"
  long long keypack_type = 0;   // Raw KeyPackType value
  long long total = 0;
  long long issued = 0;
  long long available = 0;
};

// Human-readable name for a Win32_TSLicenseKeyPack.KeyPackType value.
inline std::string keypack_type_name(const long long type) {
  switch (type) {
    case 0:
      return "unknown";
    case 1:
      return "retail";
    case 2:
      return "volume";
    case 3:
      return "concurrent";
    case 4:
      return "temporary";
    case 5:
      return "open";
    case 6:
      return "built-in";
    default:
      return "type_" + std::to_string(type);
  }
}

// True when a "Terminal Services Session" counter instance is an actual
// session (console or RDP); the "Services" instance is the aggregate for
// session 0 / system processes, not a user session.
inline bool is_session_instance(const std::string &instance) { return instance != "Services"; }

}  // namespace check_rds_internal
}  // namespace check_rds
