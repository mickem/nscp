// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/dll_defines.hpp>
#include <nscapi/protobuf/command.hpp>
#include <string>

namespace nscapi {
namespace protobuf {
namespace functions {

typedef int nagiosReturn;

// Nagios status conversion functions
NSCAPI_EXPORT int gbp_to_nagios_status(PB::Common::ResultCode ret);
NSCAPI_EXPORT PB::Common::ResultCode nagios_status_to_gpb(int ret);
NSCAPI_EXPORT PB::Common::ResultCode parse_nagios(const std::string &status);

inline PB::Common::ResultCode gbp_status_to_gbp_nagios(PB::Common::Result::StatusCodeType ret) {
  if (ret == PB::Common::Result_StatusCodeType_STATUS_OK) return PB::Common::ResultCode::OK;
  return PB::Common::ResultCode::UNKNOWN;
}
inline PB::Common::Result::StatusCodeType gbp_to_nagios_gbp_status(PB::Common::ResultCode ret) {
  if (ret == PB::Common::ResultCode::UNKNOWN || ret == PB::Common::ResultCode::WARNING || ret == PB::Common::ResultCode::CRITICAL)
    return PB::Common::Result_StatusCodeType_STATUS_ERROR;
  return PB::Common::Result_StatusCodeType_STATUS_OK;
}
}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi
