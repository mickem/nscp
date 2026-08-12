// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ad_replication_filter.hpp"

namespace ad_replication_filter {

using parsers::where::type_bool;
using parsers::where::type_date;
using parsers::where::type_int;

std::string extract_server_from_ntds_dn(const std::string &dn) {
  // The source DSA DN is "CN=NTDS Settings,CN=<server>,CN=Servers,...": the
  // server is the second RDN. Anything else (unexpected shape) is returned
  // verbatim so the operator still sees a usable identifier.
  const std::string::size_type first = dn.find(',');
  if (first == std::string::npos) return dn;
  std::string::size_type start = first + 1;
  while (start < dn.size() && dn[start] == ' ') ++start;
  const std::string::size_type end = dn.find(',', start);
  std::string rdn = dn.substr(start, end == std::string::npos ? std::string::npos : end - start);
  if (rdn.compare(0, 3, "CN=") == 0 || rdn.compare(0, 3, "cn=") == 0) return rdn.substr(3);
  return dn;
}

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("naming_context", &filter_obj::get_naming_context, "The replicated directory partition (naming context) DN")
      .add_string_var("source", &filter_obj::get_source, "The source domain controller this link replicates from")
      .add_string_var("source_dsa", &filter_obj::get_source_dsa, "Full DN of the source directory service agent")
      .add_string_var("source_address", &filter_obj::get_source_address, "Transport address of the source (GUID-based DNS name)")
      .add_string_var("last_error_message", &filter_obj::get_last_error_message, "Human readable message for the last sync result (empty when ok)");

  registry_.add_int_var("consecutive_failures", type_int, &filter_obj::get_consecutive_failures, "Number of consecutive failed sync attempts on this link")
      .add_int_perf("");
  registry_.add_int_var("last_error", type_int, &filter_obj::get_last_error, "Win32 result code of the last sync attempt (0 = success)");
  registry_.add_int_var("failed", type_bool, &filter_obj::get_failed, "True when the last sync attempt failed");
  registry_.add_int_var("last_attempt", type_date, &filter_obj::get_last_attempt, "When the last sync was attempted");
  registry_.add_int_var("last_success", type_date, &filter_obj::get_last_success, "When the last sync succeeded (epoch 0 = never)");

  registry_.add_human_string("last_attempt", &filter_obj::get_last_attempt_su, "")
      .add_human_string("last_success", &filter_obj::get_last_success_su, "");
  // clang-format on
}

}  // namespace ad_replication_filter
