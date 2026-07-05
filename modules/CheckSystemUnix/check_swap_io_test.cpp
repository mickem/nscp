// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_swap_io.h"

#include <gtest/gtest.h>

using swap_io_check::compute_swap_io;
using swap_io_check::count_swaps;
using swap_io_check::parse_vmstat_swap;
using swap_io_check::vmstat_swap;

namespace {
std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

PB::Common::ResultCode run_swap(const vmstat_swap &prev, const vmstat_swap &cur, const std::vector<std::string> &args,
                                PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_swap_io");
  for (const std::string &a : args) request.add_arguments(a);
  swap_io_check::check_swap_io_from(request, &response, prev, cur, 2.0, 1, 4096);
  return response.result();
}
}  // namespace

TEST(CheckSwapIo, ParsesVmstat) {
  const vmstat_swap s = parse_vmstat_swap("nr_free_pages 100\npswpin 500\npgfault 9\npswpout 1500\n");
  ASSERT_TRUE(s.valid);
  EXPECT_EQ(s.pswpin, 500u);
  EXPECT_EQ(s.pswpout, 1500u);
}

TEST(CheckSwapIo, MissingCounterIsInvalid) {
  EXPECT_FALSE(parse_vmstat_swap("pswpin 5\n").valid);
}

TEST(CheckSwapIo, CountsSwapDevices) {
  EXPECT_EQ(count_swaps("Filename\tType\tSize\tUsed\tPriority\n/dev/sda2 partition 1000 0 -2\n"), 1);
  EXPECT_EQ(count_swaps("Filename\tType\tSize\tUsed\tPriority\n/dev/sda2 partition 1000 0 -2\n/swapfile file 2000 0 -3\n"), 2);
  EXPECT_EQ(count_swaps("Filename\tType\tSize\tUsed\tPriority\n"), 0);
}

TEST(CheckSwapIo, ComputesRates) {
  vmstat_swap a;
  a.pswpin = 1000;
  a.pswpout = 2000;
  a.valid = true;
  vmstat_swap b;
  b.pswpin = 1200;   // +200 over 2s -> 100 pages/s
  b.pswpout = 2020;  // +20 over 2s -> 10 pages/s
  b.valid = true;

  const auto o = compute_swap_io(a, b, 2.0, 1, 4096);
  EXPECT_DOUBLE_EQ(o.swap_in, 100.0);
  EXPECT_DOUBLE_EQ(o.swap_out, 10.0);
  EXPECT_EQ(o.swap_in_bytes, 100 * 4096);
  EXPECT_EQ(o.swap_out_bytes, 10 * 4096);
  EXPECT_EQ(o.swap_count, 1);
}

TEST(CheckSwapIo, CounterResetIsZero) {
  vmstat_swap a;
  a.pswpin = 5000;
  a.valid = true;
  vmstat_swap b;  // reset
  b.valid = true;
  const auto o = compute_swap_io(a, b, 1.0, 0, 4096);
  EXPECT_DOUBLE_EQ(o.swap_in, 0.0);
}

TEST(CheckSwapIo, NoDefaultThresholdIsOk) {
  vmstat_swap a;
  a.valid = true;
  vmstat_swap b;
  b.pswpin = 1000000;  // heavy paging, but no default threshold
  b.valid = true;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_swap(a, b, {}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckSwapIo, UserThresholdTrips) {
  vmstat_swap a;
  a.valid = true;
  vmstat_swap b;
  b.pswpin = 2000;  // +2000 over 2s -> 1000 pages/s
  b.valid = true;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_swap(a, b, {"critical=swap_in > 500"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}
