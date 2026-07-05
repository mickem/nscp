// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/dll_defines.hpp>
#include <nscapi/protobuf/command.hpp>
#include <string>

namespace nscapi {
namespace protobuf {
namespace functions {

NSCAPI_EXPORT PB::Common::ResultCode parse_nagios(const std::string &status);
NSCAPI_EXPORT PB::Common::ResultCode nagios_status_to_gpb(int ret);
NSCAPI_EXPORT int gbp_to_nagios_status(PB::Common::ResultCode ret);

}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi