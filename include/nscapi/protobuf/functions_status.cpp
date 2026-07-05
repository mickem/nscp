// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <NSCAPI.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <nscapi/protobuf/functions_status.hpp>

namespace nscapi {
namespace protobuf {

PB::Common::ResultCode functions::nagios_status_to_gpb(const int ret) {
  if (ret == NSCAPI::query_return_codes::returnOK) return PB::Common::ResultCode::OK;
  if (ret == NSCAPI::query_return_codes::returnWARN) return PB::Common::ResultCode::WARNING;
  if (ret == NSCAPI::query_return_codes::returnCRIT) return PB::Common::ResultCode::CRITICAL;
  return PB::Common::ResultCode::UNKNOWN;
}

int functions::gbp_to_nagios_status(PB::Common::ResultCode ret) {
  if (ret == PB::Common::ResultCode::OK) return NSCAPI::query_return_codes::returnOK;
  if (ret == PB::Common::ResultCode::WARNING) return NSCAPI::query_return_codes::returnWARN;
  if (ret == PB::Common::ResultCode::CRITICAL) return NSCAPI::query_return_codes::returnCRIT;
  return NSCAPI::query_return_codes::returnUNKNOWN;
}

PB::Common::ResultCode functions::parse_nagios(const std::string &status) {
  const std::string lower_case_status = boost::to_lower_copy(status);
  if (lower_case_status == "o" || lower_case_status == "ok" || lower_case_status == "0") return PB::Common::ResultCode::OK;
  if (lower_case_status == "w" || lower_case_status == "warn" || lower_case_status == "warning" || lower_case_status == "1")
    return PB::Common::ResultCode::WARNING;
  if (lower_case_status == "c" || lower_case_status == "crit" || lower_case_status == "critical" || lower_case_status == "2")
    return PB::Common::ResultCode::CRITICAL;
  return PB::Common::ResultCode::UNKNOWN;
}

}  // namespace protobuf
}  // namespace nscapi
