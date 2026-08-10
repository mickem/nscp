// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include "drive_trend.hpp"

namespace {

const long long kBase = 1000000000;
const long long kInterval = 300;
const long long kRetention = 7 * 24 * 3600;
const long long kTotal = 100LL * 1024 * 1024 * 1024;

// A buffer growing 1000 bytes/second for `hours` hours at 5-minute cadence.
trend::trend_buffer growing(const long long hours) {
  trend::trend_buffer buf(kInterval, kRetention);
  const long long steps = hours * 3600 / kInterval;
  for (long long i = 0; i <= steps; ++i) buf.append(kBase + i * kInterval, 1000 * i * kInterval, kTotal);
  return buf;
}

}  // namespace

TEST(DriveTrend, LookupUnixExactAndContainingMount) {
  drive_trend::trend_map trends;
  trends["/"] = growing(6);
  trends["/home"] = growing(6);
  EXPECT_EQ(drive_trend::lookup_unix(trends, "/home"), &trends["/home"]);
  EXPECT_EQ(drive_trend::lookup_unix(trends, "/"), &trends["/"]);
  // A path below a mount resolves to the mount it lives on.
  EXPECT_EQ(drive_trend::lookup_unix(trends, "/home/user/data"), &trends["/home"]);
  EXPECT_EQ(drive_trend::lookup_unix(trends, "/var/log"), &trends["/"]);
  // Prefix must break on a path boundary: /homework is not under /home.
  EXPECT_EQ(drive_trend::lookup_unix(trends, "/homework"), &trends["/"]);
  EXPECT_EQ(drive_trend::lookup_unix(drive_trend::trend_map(), "/"), nullptr);
}

TEST(DriveTrend, LookupWinNormalizesLetterAndCase) {
  drive_trend::trend_map trends;
  trends["C:"] = growing(6);
  EXPECT_EQ(drive_trend::lookup_win(trends, "C:"), &trends["C:"]);
  EXPECT_EQ(drive_trend::lookup_win(trends, "C:\\"), &trends["C:"]);
  EXPECT_EQ(drive_trend::lookup_win(trends, "c:\\"), &trends["C:"]);
  EXPECT_EQ(drive_trend::lookup_win(trends, "D:\\"), nullptr);
  // Un-lettered volumes have no collector row.
  EXPECT_EQ(drive_trend::lookup_win(trends, ""), nullptr);
}

TEST(DriveTrend, ComputeAndProjection) {
  const trend::trend_buffer buf = growing(6);
  const long long now = buf.newest_ts();
  const trend::slope_result r = drive_trend::compute(&buf, now, 24 * 3600);
  ASSERT_TRUE(r.valid);
  const auto rate = drive_trend::rate_per_day(r);
  ASSERT_TRUE(rate);
  EXPECT_EQ(*rate, 1000 * 24 * 3600);
  const auto full_in = drive_trend::full_in(r, 3600000, kTotal);
  ASSERT_TRUE(full_in);
  EXPECT_EQ(*full_in, 3600);
  // No history at all: invalid slope, everything unset.
  const trend::slope_result none = drive_trend::compute(nullptr, now, 24 * 3600);
  EXPECT_FALSE(none.valid);
  EXPECT_EQ(none.samples, 0);
  EXPECT_FALSE(drive_trend::rate_per_day(none));
  EXPECT_FALSE(drive_trend::full_in(none, 3600000, kTotal));
  // Unreadable row (size 0): no projection even with a valid slope.
  EXPECT_FALSE(drive_trend::full_in(r, 0, 0));
}

TEST(DriveTrend, TotalRowTakesMinFullInAndSumsRates) {
  drive_trend::total_values total;
  total.append(boost::optional<long long>(7200), boost::optional<long long>(500), 3600, 12);
  total.append(boost::optional<long long>(3600), boost::optional<long long>(1500), 7200, 24);
  total.append(boost::none, boost::none, 0, 0);  // a drive with no trend contributes nothing
  ASSERT_TRUE(total.full_in);
  EXPECT_EQ(*total.full_in, 3600);  // the soonest-full drive wins
  ASSERT_TRUE(total.rate);
  EXPECT_EQ(*total.rate, 2000);  // rates pool additively
  EXPECT_EQ(total.span, 7200);
  EXPECT_EQ(total.samples, 36);
  // A shrinking-only fleet: no full_in, negative pooled rate.
  drive_trend::total_values shrinking;
  shrinking.append(boost::none, boost::optional<long long>(-500), 3600, 12);
  EXPECT_FALSE(shrinking.full_in);
  ASSERT_TRUE(shrinking.rate);
  EXPECT_EQ(*shrinking.rate, -500);
}

TEST(DriveTrend, HumanRenderings) {
  EXPECT_EQ(drive_trend::format_full_in(boost::none), "never");
  EXPECT_EQ(drive_trend::format_full_in(boost::optional<long long>(3 * 24 * 3600 + 4 * 3600)), "3d 04:00");
  EXPECT_EQ(drive_trend::format_rate(boost::none), "unknown");
  EXPECT_EQ(drive_trend::format_rate(boost::optional<long long>(12933429657LL)), "12.045GB/day");
  EXPECT_EQ(drive_trend::format_rate(boost::optional<long long>(-1024 * 1024)), "-1MB/day");
}
