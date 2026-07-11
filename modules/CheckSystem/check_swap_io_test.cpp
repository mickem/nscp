// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_swap_io.hpp"

#include <gtest/gtest.h>

using swap_io_check::make_swap_obj;
using swap_io_check::swap_obj;

namespace {
std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

PB::Common::ResultCode run_swap(double pages_in, double pages_out, long long swap_count, long long page_size, const std::vector<std::string> &args,
                                PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_swap_io");
  for (const std::string &a : args) request.add_arguments(a);
  swap_io_check::check_swap_io_from(request, &response, pages_in, pages_out, swap_count, page_size);
  return response.result();
}
}  // namespace

TEST(CheckSwapIoWin, ComputesBytesFromPageRate) {
  const swap_obj o = make_swap_obj(100.0, 10.0, 2, 4096);
  EXPECT_DOUBLE_EQ(o.swap_in, 100.0);
  EXPECT_DOUBLE_EQ(o.swap_out, 10.0);
  EXPECT_EQ(o.swap_in_bytes, 100 * 4096);
  EXPECT_EQ(o.swap_out_bytes, 10 * 4096);
  EXPECT_EQ(o.swap_count, 2);
}

TEST(CheckSwapIoWin, NegativeRateClampsToZero) {
  // PDH can briefly report a negative rate around a counter wrap/reset.
  const swap_obj o = make_swap_obj(-5.0, -1.0, 1, 4096);
  EXPECT_DOUBLE_EQ(o.swap_in, 0.0);
  EXPECT_DOUBLE_EQ(o.swap_out, 0.0);
  EXPECT_EQ(o.swap_in_bytes, 0);
  EXPECT_EQ(o.swap_out_bytes, 0);
}

TEST(CheckSwapIoWin, NoDefaultThresholdIsOk) {
  PB::Commands::QueryResponseMessage::Response response;
  // Heavy paging, but there is no default threshold, so it must stay OK.
  EXPECT_EQ(run_swap(1000000.0, 1000000.0, 1, 4096, {}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckSwapIoWin, UserThresholdTrips) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_swap(1000.0, 0.0, 1, 4096, {"critical=swap_in > 500"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST(CheckSwapIoWin, ByteThresholdUsesPageSize) {
  PB::Commands::QueryResponseMessage::Response response;
  // 200 pages/s * 4096 = 819200 bytes/s, over a 500000 byte/s critical bound.
  EXPECT_EQ(run_swap(200.0, 0.0, 1, 4096, {"critical=swap_in_bytes > 500000"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}
