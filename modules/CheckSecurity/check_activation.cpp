// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_activation.hpp"

#include <boost/algorithm/string.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

namespace po = boost::program_options;

namespace activation_filter {

const char *windows_application_id = "55c92734-d682-4d71-983e-d6ec3f16059f";

std::string license_status_name(const long long status) {
  switch (status) {
    case status_unlicensed:
      return "unlicensed";
    case status_licensed:
      return "licensed";
    case status_oob_grace:
      return "initial_grace";
    case status_oot_grace:
      return "additional_grace";
    case status_non_genuine_grace:
      return "non_genuine_grace";
    case status_notification:
      return "notification";
    case status_extended_grace:
      return "extended_grace";
    default:
      return "unknown";
  }
}

std::string genuine_state_name(const long long state) {
  switch (state) {
    case genuine_is_genuine:
      return "genuine";
    case genuine_invalid_license:
      return "invalid_license";
    case genuine_tampered:
      return "tampered";
    case genuine_offline:
      return "offline";
    default:
      return "unknown";
  }
}

long long grace_minutes_to_days(const long long minutes) {
  if (minutes <= 0) return 0;
  return minutes / (60 * 24);
}

void finalize(filter_obj &obj) {
  obj.licensed = (obj.license_status == status_licensed) ? 1 : 0;
  obj.genuine = (obj.genuine_status == genuine_is_genuine) ? 1 : 0;
  obj.grace_days = grace_minutes_to_days(obj.grace_minutes);
  obj.is_windows = boost::iequals(obj.app_id, windows_application_id) ? 1 : 0;
}

using parsers::where::type_bool;
using parsers::where::type_int;
filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("name", &filter_obj::get_name, "Product name, e.g. 'Windows(R), Professional edition'")
      .add_string_var("description", &filter_obj::get_description, "Product description including the licensing channel")
      .add_string_var("id", &filter_obj::get_id, "Product SKU id (GUID)")
      .add_string_var("key", &filter_obj::get_key, "Partial product key (the last five characters of the installed key)")
      .add_string_var("channel", &filter_obj::get_channel, "Product key channel: Retail, Volume:MAK, Volume:GVLK, OEM, ...")
      .add_string_var("activation_status", &filter_obj::get_status, "Licensing status as a word: licensed, unlicensed, initial_grace, additional_grace, non_genuine_grace, notification, extended_grace")
      .add_string_var("status", &filter_obj::get_status, "Deprecated alias for activation_status (the name clashes with the generic status summary keyword)")
      .add_string_var("genuine_state", &filter_obj::get_genuine_state, "Genuine status as a word: genuine, invalid_license, tampered, offline, unknown");
  registry_.add_int_var("licensed", type_bool, &filter_obj::get_licensed, "True when the product is fully licensed (activated)")
      .no_perf()
      .add_int_var("genuine", type_bool, &filter_obj::get_genuine, "True when Windows reports itself as genuine (false also when it could not be determined)")
      .no_perf()
      .add_int_var("is_windows", type_bool, &filter_obj::get_is_windows, "True when the product is Windows itself (as opposed to another licensed product)")
      .no_perf()
      .add_int_var("license_status", type_int, &filter_obj::get_license_status, "Raw LicenseStatus: 0 unlicensed, 1 licensed, 2 initial grace, 3 additional grace, 4 non-genuine grace, 5 notification, 6 extended grace")
      .no_perf()
      .add_int_var("license_status_reason", type_int, &filter_obj::get_license_status_reason, "Raw LicenseStatusReason code explaining the status")
      .no_perf()
      .add_int_var("grace_days", type_int, &filter_obj::get_grace_days, "Remaining grace/renewal period in whole days (0 when no grace period applies)")
      .add_int_perf("d", "", "_grace")
      .add_int_var("grace_minutes", type_int, &filter_obj::get_grace_minutes, "Remaining grace/renewal period in minutes, as Windows reports it (0 when no grace period applies)")
      .no_perf();
  // clang-format on
}
}  // namespace activation_filter

namespace check_activation_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<activation_filter::filter> filter_helper(request, response, data);

  bool all_products = false;
  bool skip_genuine = false;

  activation_filter::filter filter;
  // Default: CRITICAL when the product is not activated (unlicensed, in grace or
  // in the notification/nag state), WARNING while a grace or KMS renewal period
  // is running out. grace_days is 0 on a permanently activated host, so the
  // warning only fires where a countdown is actually ticking.
  filter_helper.add_options("grace_days > 0 and grace_days < 30", "licensed = 0", "", filter.get_filter_syntax(), "unknown");
  // The perf label is a fixed word (there is normally exactly one row); pass
  // perf-syntax=${name} to tell several products apart with all-products=true.
  filter_helper.add_syntax("${status}: ${list}", "${name}: ${activation_status} (${genuine_state}, grace ${grace_days}d)", "license",
                           "%(status): No licensing information found (Software Licensing service unavailable?)", "%(status): ${list}");

  // clang-format off
  filter_helper.get_desc().add_options()
    ("all-products", po::value<bool>(&all_products)->implicit_value(true)->default_value(false),
     "Report every licensed product with an installed key (Office, ...) instead of only Windows itself.")
    ("skip-genuine", po::value<bool>(&skip_genuine)->implicit_value(true)->default_value(false),
     "Do not evaluate the genuine state (skips the SLIsGenuineLocal call); genuine_state then reads 'unknown'.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<activation_filter::filter_obj_ptr> products;
  std::string error;
  activation_source::gather(all_products, !skip_genuine, products, error);
  if (!error.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, error);
  }

  parsers::where::constants::reset();
  for (const activation_filter::filter_obj_ptr &p : products) filter.match(p);
  filter_helper.post_process(filter);
}

}  // namespace check_activation_command
