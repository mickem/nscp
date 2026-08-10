// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>
#include <trend/trend_buffer.hpp>

namespace {

const long long kInterval = 300;             // 5 minutes
const long long kRetention = 7 * 24 * 3600;  // 7 days
const long long kBase = 1000000000;          // arbitrary epoch base
const long long kTotal = 100LL * 1024 * 1024 * 1024;
const long long kDay = 24 * 3600;

// Fill a buffer with one sample per interval for `count` samples where the
// value at step i is value(i).
template <class F>
trend::trend_buffer fill(const long long count, F value, const long long total = kTotal) {
  trend::trend_buffer buf(kInterval, kRetention);
  for (long long i = 0; i < count; ++i) buf.append(kBase + i * kInterval, value(i), total);
  return buf;
}

}  // namespace

TEST(TrendBuffer, LinearGrowthYieldsExactSlope) {
  // 1000 bytes per second, sampled every 5 minutes for 6 hours.
  const auto buf = fill(72, [](long long i) { return 1000 * i * kInterval; });
  const long long now = kBase + 71 * kInterval;
  const auto r = buf.slope_over(now, kDay);
  ASSERT_TRUE(r.valid);
  EXPECT_NEAR(r.slope, 1000.0, 1e-6);
  EXPECT_EQ(r.samples, 72);
  EXPECT_EQ(r.span, 71 * kInterval);
}

TEST(TrendBuffer, ProjectZeroFromCurrentFree) {
  const auto buf = fill(72, [](long long i) { return 1000 * i * kInterval; });
  const auto r = buf.slope_over(kBase + 71 * kInterval, kDay);
  ASSERT_TRUE(r.valid);
  const auto full_in = trend::trend_buffer::project_zero(3600 * 1000, r);
  ASSERT_TRUE(full_in);
  EXPECT_EQ(*full_in, 3600);
}

TEST(TrendBuffer, ShrinkingSeriesNeverFills) {
  const auto buf = fill(72, [](long long i) { return 1000000000 - 1000 * i * kInterval; });
  const auto r = buf.slope_over(kBase + 71 * kInterval, kDay);
  ASSERT_TRUE(r.valid);
  EXPECT_LT(r.slope, 0.0);
  EXPECT_FALSE(trend::trend_buffer::project_zero(1000000, r));
}

TEST(TrendBuffer, FlatSeriesNeverFills) {
  const auto buf = fill(72, [](long long) { return 42; });
  const auto r = buf.slope_over(kBase + 71 * kInterval, kDay);
  ASSERT_TRUE(r.valid);
  EXPECT_NEAR(r.slope, 0.0, 1e-9);
  EXPECT_FALSE(trend::trend_buffer::project_zero(1000000, r));
}

TEST(TrendBuffer, TooFewSamplesIsInvalid) {
  const auto buf = fill(2, [](long long i) { return 1000 * i; });
  const auto r = buf.slope_over(kBase + kInterval, kDay);
  EXPECT_FALSE(r.valid);
  EXPECT_EQ(r.samples, 2);
  EXPECT_FALSE(trend::trend_buffer::project_zero(1000000, r));
}

TEST(TrendBuffer, TooShortSpanIsInvalid) {
  // 3 samples only 2 intervals apart: below the 3x-interval minimum span.
  const auto buf = fill(3, [](long long i) { return 1000 * i; });
  const auto r = buf.slope_over(kBase + 2 * kInterval, kDay);
  EXPECT_FALSE(r.valid);
  EXPECT_EQ(r.samples, 3);
  EXPECT_EQ(r.span, 2 * kInterval);
}

TEST(TrendBuffer, FourConsecutiveSamplesAreEnough) {
  const auto buf = fill(4, [](long long i) { return 1000 * i * kInterval; });
  const auto r = buf.slope_over(kBase + 3 * kInterval, kDay);
  ASSERT_TRUE(r.valid);
  EXPECT_NEAR(r.slope, 1000.0, 1e-6);
}

TEST(TrendBuffer, SawtoothMeasuresNetGrowthOverLongWindow) {
  // Grows 600 bytes/s but drops 80% of accumulated value every 2 hours
  // (log rotation): the net trend across many teeth is far below the burst
  // rate inside one tooth.
  trend::trend_buffer buf(kInterval, kRetention);
  long long value = 0;
  const long long steps_per_tooth = 24;  // 2h / 5m
  for (long long i = 0; i < 24 * 12; ++i) {  // 24 hours
    if (i > 0 && i % steps_per_tooth == 0) value = value / 5;
    value += 600 * kInterval;
    buf.append(kBase + i * kInterval, value, kTotal);
  }
  const long long now = kBase + (24 * 12 - 1) * kInterval;
  const auto day = buf.slope_over(now, kDay);
  ASSERT_TRUE(day.valid);
  EXPECT_LT(day.slope, 300.0);  // well below the burst rate
  // A short window inside one tooth sees the burst rate instead.
  const auto burst = buf.slope_over(now, 90 * 60);
  ASSERT_TRUE(burst.valid);
  EXPECT_NEAR(burst.slope, 600.0, 60.0);
}

TEST(TrendBuffer, WindowSelectsRecentSamplesOnly) {
  // 12h flat, then 12h of steady growth: a 6h window sees pure growth.
  trend::trend_buffer buf(kInterval, kRetention);
  for (long long i = 0; i < 24 * 12; ++i) {
    const long long value = i < 12 * 12 ? 1000 : (i - 12 * 12) * 500 * kInterval;
    buf.append(kBase + i * kInterval, value, kTotal);
  }
  const long long now = kBase + (24 * 12 - 1) * kInterval;
  const auto r = buf.slope_over(now, 6 * 3600);
  ASSERT_TRUE(r.valid);
  EXPECT_NEAR(r.slope, 500.0, 1e-3);
  const auto full = buf.slope_over(now, kDay);
  ASSERT_TRUE(full.valid);
  EXPECT_LT(full.slope, 500.0);
}

TEST(TrendBuffer, CadenceSubsampling) {
  // Offered every 10 seconds, kept every 5 minutes.
  trend::trend_buffer buf(kInterval, kRetention);
  for (long long t = 0; t < 3600; t += 10) buf.append(kBase + t, t, kTotal);
  EXPECT_EQ(buf.size(), 12u);  // one per 5 minutes over 1 hour
}

TEST(TrendBuffer, BackwardsTimestampIgnored) {
  trend::trend_buffer buf(kInterval, kRetention);
  buf.append(kBase, 1, kTotal);
  buf.append(kBase - 3600, 2, kTotal);  // clock stepped back: ignored
  EXPECT_EQ(buf.size(), 1u);
  EXPECT_EQ(buf.newest_ts(), kBase);
  buf.append(kBase + kInterval, 3, kTotal);
  EXPECT_EQ(buf.size(), 2u);
}

TEST(TrendBuffer, GapsAreHandledNatively) {
  // 2 hours of samples, a 12 hour gap, 2 more hours: slope still exact for
  // a linear series (OLS over irregular timestamps needs no interpolation).
  trend::trend_buffer buf(kInterval, kRetention);
  for (long long i = 0; i < 24; ++i) buf.append(kBase + i * kInterval, 100 * (i * kInterval), kTotal);
  const long long resume = kBase + 14 * 3600;
  for (long long i = 0; i < 24; ++i) buf.append(resume + i * kInterval, 100 * (resume - kBase + i * kInterval), kTotal);
  const auto r = buf.slope_over(resume + 23 * kInterval, kDay);
  ASSERT_TRUE(r.valid);
  EXPECT_NEAR(r.slope, 100.0, 1e-6);
}

TEST(TrendBuffer, ResizeResetsHistory) {
  trend::trend_buffer buf(kInterval, kRetention);
  for (long long i = 0; i < 24; ++i) buf.append(kBase + i * kInterval, 1000 * i, kTotal);
  EXPECT_EQ(buf.size(), 24u);
  // Filesystem grown: everything before the resize is garbage.
  buf.append(kBase + 24 * kInterval, 1000 * 24, kTotal * 2);
  EXPECT_EQ(buf.size(), 1u);
  EXPECT_EQ(buf.context(), kTotal * 2);
}

TEST(TrendBuffer, RetentionTrimsOldSamples) {
  trend::trend_buffer buf(kInterval, 3600);  // 1 hour retention
  for (long long i = 0; i < 48; ++i) buf.append(kBase + i * kInterval, i, kTotal);
  EXPECT_EQ(buf.size(), 13u);  // newest sample plus one hour of history
  EXPECT_EQ(buf.newest_ts(), kBase + 47 * kInterval);
}

TEST(TrendBuffer, EncodeDecodeRoundTrip) {
  const auto buf = fill(48, [](long long i) { return 12345 + 1000 * i * kInterval; });
  const std::string data = buf.encode();
  const long long now = kBase + 47 * kInterval;
  const auto copy = trend::trend_buffer::decode(data, kInterval, kRetention, now);
  EXPECT_EQ(copy.size(), buf.size());
  EXPECT_EQ(copy.context(), buf.context());
  const auto a = buf.slope_over(now, kDay);
  const auto b = copy.slope_over(now, kDay);
  ASSERT_TRUE(a.valid);
  ASSERT_TRUE(b.valid);
  EXPECT_NEAR(a.slope, b.slope, 1e-9);
}

TEST(TrendBuffer, EncodeDownsamplesButKeepsNewest) {
  const auto buf = fill(48, [](long long i) { return 1000 * i; });  // 4 hours at 5 min
  const std::string data = buf.encode(30 * 60);                    // 30 min granularity
  const long long now = kBase + 47 * kInterval;
  const auto copy = trend::trend_buffer::decode(data, kInterval, kRetention, now);
  EXPECT_LT(copy.size(), buf.size());
  EXPECT_GE(copy.size(), 8u);
  EXPECT_EQ(copy.newest_ts(), buf.newest_ts());
}

TEST(TrendBuffer, DecodeDiscardsFutureAndStaleSamples) {
  const auto buf = fill(48, [](long long i) { return 1000 * i; });
  const std::string data = buf.encode();
  // "Now" rewound to the middle of the series: the later half is in the
  // future (clock stepped back since the save) and must be dropped.
  const long long now = kBase + 23 * kInterval;
  const auto copy = trend::trend_buffer::decode(data, kInterval, kRetention, now);
  EXPECT_EQ(copy.size(), 24u);
  EXPECT_LE(copy.newest_ts(), now);
  // "Now" far ahead: everything is stale.
  const auto stale = trend::trend_buffer::decode(data, kInterval, 3600, kBase + 30 * kDay);
  EXPECT_TRUE(stale.empty());
}

TEST(TrendBuffer, DecodeGarbageYieldsEmptyBuffer) {
  const long long now = kBase;
  EXPECT_TRUE(trend::trend_buffer::decode("", kInterval, kRetention, now).empty());
  EXPECT_TRUE(trend::trend_buffer::decode("2|0|1:2", kInterval, kRetention, now).empty());        // unknown version
  EXPECT_TRUE(trend::trend_buffer::decode("1|abc|1:2", kInterval, kRetention, now).empty());      // bad context
  EXPECT_TRUE(trend::trend_buffer::decode("1|0|no-sep", kInterval, kRetention, now).empty());     // bad sample
  EXPECT_TRUE(trend::trend_buffer::decode("1|0|5:1|x:y", kInterval, kRetention, now).empty());    // bad delta
  EXPECT_TRUE(trend::trend_buffer::decode("1|0|5:1|-10:0", kInterval, kRetention, now).empty());  // non-monotonic
}

TEST(TrendBuffer, DecodedBufferKeepsCollectingAndResizeStillResets) {
  const auto buf = fill(48, [](long long i) { return 1000 * i; });
  const long long now = kBase + 47 * kInterval;
  auto copy = trend::trend_buffer::decode(buf.encode(), kInterval, kRetention, now);
  copy.append(now + kInterval, 48000, kTotal);
  EXPECT_EQ(copy.size(), 49u);
  copy.append(now + 2 * kInterval, 49000, kTotal + 1);
  EXPECT_EQ(copy.size(), 1u);
}
