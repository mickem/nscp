// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_defender.hpp"

#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

namespace defender_filter {
using parsers::where::type_bool;
using parsers::where::type_int;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_int_var("enabled", type_bool, &filter_obj::get_enabled, "True if Defender antivirus/service is enabled")
      .no_perf()
      .add_int_var("realtime_enabled", type_bool, &filter_obj::get_realtime_enabled, "True if real-time protection is on")
      .no_perf()
      .add_int_var("tamper_protection", type_bool, &filter_obj::get_tamper_protection, "True if tamper protection is on")
      .no_perf()
      .add_int_var("signature_age", type_int, &filter_obj::get_signature_age, "Antivirus signature age in days (-1 if unknown)")
      .add_int_perf("", "", "_signature_age")
      .add_int_var("quick_scan_age", type_int, &filter_obj::get_quick_scan_age, "Days since the last quick scan (-1 if never/unknown)")
      .add_int_perf("", "", "_quick_scan_age")
      .add_int_var("full_scan_age", type_int, &filter_obj::get_full_scan_age, "Days since the last full scan (-1 if never/unknown)")
      .add_int_perf("", "", "_full_scan_age");
  registry_.add_string_var("engine_version", &filter_obj::get_engine_version, "Defender anti-malware engine version")
      .add_string_var("signature_version", &filter_obj::get_signature_version, "Antivirus signature (definition) version")
      .add_string_var("product_version", &filter_obj::get_product_version, "Defender platform/product version");
  // clang-format on
}
}  // namespace defender_filter

namespace check_defender_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<defender_filter::filter> filter_helper(request, response, data);

  defender_filter::filter filter;
  // Default: WARNING when definitions are getting old; CRITICAL when protection
  // is off or definitions are badly stale. A negative (unknown) age never trips
  // these. empty_state = "unknown": when Defender is not the active AV the query
  // returns no rows and the check reports UNKNOWN rather than a hard error.
  filter_helper.add_options("signature_age > 3", "enabled = 0 or realtime_enabled = 0 or signature_age > 7", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}",
                           "Defender enabled=${enabled} realtime=${realtime_enabled} tamper=${tamper_protection} sig_age=${signature_age}d "
                           "sig=${signature_version} engine=${engine_version}",
                           "defender", "%(status): Microsoft Defender status unavailable (not installed or another antivirus is active)",
                           "%(status): Microsoft Defender healthy (signature age ${signature_age}d)");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<defender_filter::filter_obj_ptr> rows;
  std::string error;
  defender_source::gather(rows, error);
  if (!error.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, error);
  }

  parsers::where::constants::reset();
  for (const defender_filter::filter_obj_ptr &r : rows) filter.match(r);
  filter_helper.post_process(filter);
}

}  // namespace check_defender_command
