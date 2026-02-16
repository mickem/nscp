/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
