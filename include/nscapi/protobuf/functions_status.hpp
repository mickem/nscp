/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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
