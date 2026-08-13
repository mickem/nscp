// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_ad_replication.hpp"

#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>
#include <string>
#include <vector>

#include "ad_replication_filter.hpp"
#include "ad_replication_source.hpp"

namespace po = boost::program_options;

namespace check_ad_replication_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<ad_replication_filter::filter> filter_helper(request, response, data);

  std::string server;
  int timeout_ms = 5000;

  ad_replication_filter::filter filter;
  // A link is suspect on its first failed sync and clearly broken after
  // several in a row or a day without a successful sync. A never-synced link
  // (last_success = epoch 0) trips the 24h rule by design.
  filter_helper.add_options("consecutive_failures > 0", "consecutive_failures > 4 or last_success < -24h", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${problem_list}", "${source} ${naming_context}: ${consecutive_failures} failures, last success ${last_success}",
                           "${source} ${naming_context}", "No replication partners found (single domain controller?)",
                           "%(status): all %(count) replication links are healthy");
  // Thresholding on the date keywords must not spray epoch-seconds perfdata;
  // consecutive_failures (registered with perf) is the useful series.
  filter_helper.set_default_perf_config("last_success(ignored:true)last_attempt(ignored:true)");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("server", po::value<std::string>(&server), "The domain controller to check (default: the local machine).")
    ("timeout", po::value<int>(&timeout_ms)->default_value(5000),
        "Timeout in milliseconds for reaching a remote server= before binding to it (ignored for the local machine).")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<ad_replication_filter::filter_obj_ptr> neighbors;
  std::string error;
  bool not_a_dc = false;
  if (!ad_replication_source::fetch(server, timeout_ms, neighbors, error, not_a_dc)) {
    // Both branches are UNKNOWN; the message distinguishes "this host is not a
    // domain controller" (safe to deploy fleet-wide) from a real read failure.
    return nscapi::protobuf::functions::set_response_bad(*response, not_a_dc ? "Not a domain controller: " + error : error);
  }

  parsers::where::constants::reset();
  for (const ad_replication_filter::filter_obj_ptr &n : neighbors) filter.match(n);
  filter_helper.post_process(filter);
}

}  // namespace check_ad_replication_command
