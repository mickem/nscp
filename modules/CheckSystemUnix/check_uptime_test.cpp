// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_uptime.h"

#include <gtest/gtest.h>
#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <string>
#include <vector>

namespace {

using checks::check_uptime_filter::filter_obj;

// Drive the uptime filter the way check_uptime does, over a fabricated
// uptime, and return the check result.
PB::Common::ResultCode run_uptime_filter(const std::vector<std::string> &args, const long long uptime_secs) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_uptime");
  for (const std::string &a : args) request.add_arguments(a);
  PB::Commands::QueryResponseMessage::Response response;
  modern_filter::data_container data;
  modern_filter::cli_helper<checks::check_uptime_filter::filter> filter_helper(request, &response, data);
  checks::check_uptime_filter::filter filter;
  filter_helper.add_options("uptime < 2d", "uptime < 1d", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${list}", "uptime: ${uptime}", "uptime", "", "");
  if (!filter_helper.parse_options()) return PB::Common::ResultCode::UNKNOWN;
  if (!filter_helper.build_filter(filter)) return PB::Common::ResultCode::UNKNOWN;
  const long long now = 1755900000;
  std::shared_ptr<filter_obj> record(
      new filter_obj(uptime_secs, now, boost::posix_time::from_time_t(now - uptime_secs), ""));
  filter.match(record);
  filter_helper.post_process(filter);
  return response.result();
}

}  // namespace

TEST(CheckUptimeFilter, WholeNumberDurationThreshold) {
  // 2h = 7200s: below fires, above does not.
  EXPECT_EQ(run_uptime_filter({"warning=none", "critical=uptime < 2h"}, 7000), PB::Common::ResultCode::CRITICAL);
  EXPECT_EQ(run_uptime_filter({"warning=none", "critical=uptime < 2h"}, 7500), PB::Common::ResultCode::OK);
}

TEST(CheckUptimeFilter, FractionalDurationThresholdIsNotTruncated) {
  // 2.5h = 9000s. The converter used to truncate the count to 2h (7200s)
  // through the int accessor, so an uptime of 7800s (between the two) tells
  // the paths apart: correct scaling fires, the truncated form did not.
  EXPECT_EQ(run_uptime_filter({"warning=none", "critical=uptime < 2.5h"}, 7800), PB::Common::ResultCode::CRITICAL);
  // ...and above the real threshold nothing fires.
  EXPECT_EQ(run_uptime_filter({"warning=none", "critical=uptime < 2.5h"}, 9500), PB::Common::ResultCode::OK);
}

TEST(CheckUptimeFilter, PlainSecondsThreshold) {
  EXPECT_EQ(run_uptime_filter({"warning=none", "critical=uptime < 600"}, 500), PB::Common::ResultCode::CRITICAL);
  EXPECT_EQ(run_uptime_filter({"warning=none", "critical=uptime < 600"}, 700), PB::Common::ResultCode::OK);
}
