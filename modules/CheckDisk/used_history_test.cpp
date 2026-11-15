// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "used_history.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

const long long kTotal = 100LL * 1024 * 1024 * 1024;
// An arbitrary monotonic origin: the clock is only ever compared with itself.
const long long kStart = 123456;

// A volume that gains `per_second` bytes a second, sampled every `step`
// seconds for `seconds` seconds, starting at kStart with `used0` used.
used_history::volume_history growing(const long long seconds, const long long step, const long long per_second, const long long used0 = 1000000) {
  used_history::volume_history h;
  for (long long t = 0; t <= seconds; t += step) h.add(kStart + t, used0 + per_second * t, kTotal);
  return h;
}

std::vector<used_history::reading> one(const std::string &name, const long long used, const long long total = kTotal) {
  return {used_history::reading{name, used, total}};
}

}  // namespace

TEST(UsedHistory, NothingUntilAFullHourHasPassed) {
  used_history::volume_history h;
  for (long long t = 0; t < 3600; t += 10) {
    h.add(kStart + t, 1000 + t, kTotal);
    const used_history::changes c = h.get();
    EXPECT_FALSE(c.change_1h) << "at " << t;
    EXPECT_FALSE(c.change_24h) << "at " << t;
  }
  // The first tick an hour after the first sample.
  h.add(kStart + 3600, 1000 + 3600, kTotal);
  const used_history::changes c = h.get();
  ASSERT_TRUE(c.change_1h);
  EXPECT_EQ(c.change_1h.value(), 3600);
  EXPECT_FALSE(c.change_24h);
}

TEST(UsedHistory, TheHourIsMeasuredAgainstTheNewestSampleAnHourOld) {
  // 10-second ticks for three hours, growing 1000 bytes a second: the minute
  // ring holds one sample a minute, so the reference is 60 to 61 minutes old
  // and the change is at least an hour's growth and at most 61 minutes'.
  for (long long seconds = 3 * 3600; seconds < 3 * 3600 + 120; seconds += 10) {
    const used_history::volume_history h = growing(seconds, 10, 1000);
    const used_history::changes c = h.get();
    ASSERT_TRUE(c.change_1h) << seconds;
    EXPECT_GE(c.change_1h.value(), 3600 * 1000) << seconds;
    EXPECT_LE(c.change_1h.value(), 3660 * 1000) << seconds;
  }
}

TEST(UsedHistory, TheDayAppearsAfterADayAndCoversADayToADayAndAnHour) {
  used_history::volume_history h;
  long long t = 0;
  for (; t < 24 * 3600; t += 10) {
    h.add(kStart + t, 5000000 + t, kTotal);
    ASSERT_FALSE(h.get().change_24h) << t;
  }
  h.add(kStart + t, 5000000 + t, kTotal);
  ASSERT_TRUE(h.get().change_24h);
  EXPECT_EQ(h.get().change_24h.value(), 24 * 3600);
  // Two days on, the reference is between 24 and 25 hours old.
  for (; t < 48 * 3600; t += 10) {
    h.add(kStart + t, 5000000 + t, kTotal);
    const used_history::changes c = h.get();
    ASSERT_TRUE(c.change_24h) << t;
    EXPECT_GE(c.change_24h.value(), 24 * 3600) << t;
    EXPECT_LE(c.change_24h.value(), 25 * 3600 + 10) << t;
  }
}

TEST(UsedHistory, FreeingSpaceIsANegativeChangeAndAFlatVolumeIsZero) {
  const used_history::changes shrinking = growing(2 * 3600, 10, -100, 100000000).get();
  ASSERT_TRUE(shrinking.change_1h);
  EXPECT_LT(shrinking.change_1h.value(), 0);
  const used_history::changes flat = growing(2 * 3600, 10, 0).get();
  ASSERT_TRUE(flat.change_1h);
  EXPECT_EQ(flat.change_1h.value(), 0);
}

TEST(UsedHistory, TheRingsStayBounded) {
  const used_history::volume_history h = growing(3 * 24 * 3600, 10, 1);
  // One sample a minute back to the first that is an hour old.
  EXPECT_LE(h.minutes().size(), 62u);
  EXPECT_GE(h.minutes().size(), 60u);
  // One an hour back to the first that is a day old.
  EXPECT_LE(h.hours().size(), 26u);
  EXPECT_GE(h.hours().size(), 24u);
  EXPECT_LE(h.minutes().size(), h.minutes().capacity());
  EXPECT_LE(h.hours().size(), h.hours().capacity());
}

TEST(UsedHistory, AGapInSamplingDoesNotStandInForTheWindow) {
  used_history::volume_history h;
  h.add(kStart, 1000, kTotal);
  // The collector stopped for two hours: the only old sample is two hours
  // old, which is not "the last hour".
  h.add(kStart + 2 * 3600, 9000, kTotal);
  EXPECT_FALSE(h.get().change_1h);
  // Within the slack it still counts.
  used_history::volume_history late;
  late.add(kStart, 1000, kTotal);
  late.add(kStart + 3600 + used_history::slack_1h, 4000, kTotal);
  ASSERT_TRUE(late.get().change_1h);
  EXPECT_EQ(late.get().change_1h.value(), 3000);
}

TEST(UsedHistory, AResizedVolumeStartsOver) {
  used_history::volume_history h = growing(2 * 3600, 10, 10);
  ASSERT_TRUE(h.get().change_1h);
  h.add(kStart + 2 * 3600 + 10, 5, kTotal * 2);
  EXPECT_FALSE(h.get().change_1h);
}

TEST(UsedHistory, AClockThatGoesBackStartsTheRingOver) {
  used_history::ring r(60, 3600);
  r.add({1000, 1});
  r.add({1060, 2});
  r.add({500, 3});
  EXPECT_EQ(r.size(), 1u);
}

TEST(UsedHistory, TrackerDropsVolumesThatDisappearAndBoundsTheCount) {
  used_history::tracker t;
  t.update(kStart, {{"C:", 10, kTotal}, {"D:", 20, kTotal}});
  EXPECT_EQ(t.size(), 2u);
  t.update(kStart + 10, one("C:", 11));
  EXPECT_EQ(t.size(), 1u);
  EXPECT_EQ(t.find("D:"), nullptr);
  // D: is back, with no history: nothing for an hour.
  t.update(kStart + 20, {{"C:", 12, kTotal}, {"D:", 20, kTotal}});
  for (long long s = 30; s <= 3600; s += 10) t.update(kStart + s, {{"C:", 12, kTotal}, {"D:", 20, kTotal}});
  const used_history::change_map m = t.get();
  ASSERT_TRUE(m.at("C:").change_1h);
  EXPECT_FALSE(m.at("D:").change_1h);

  std::vector<used_history::reading> many;
  for (std::size_t i = 0; i < used_history::max_volumes + 10; ++i) many.push_back({"/mnt/" + std::to_string(i), 1, kTotal});
  used_history::tracker big;
  big.update(kStart, many);
  EXPECT_EQ(big.size(), used_history::max_volumes);
}

TEST(UsedHistory, AVolumeWithNoSizeIsKeptButNotSampled) {
  used_history::tracker t;
  t.update(kStart, one("/", 100));
  t.update(kStart + 10, one("/", 0, 0));
  ASSERT_NE(t.find("/"), nullptr);
  // ...and a new volume with no size is not started.
  t.update(kStart + 20, {{"/", 100, kTotal}, {"/new", 0, 0}});
  EXPECT_EQ(t.find("/new"), nullptr);
  for (long long s = 30; s <= 3600; s += 10) t.update(kStart + s, one("/", 400));
  ASSERT_TRUE(t.get().at("/").change_1h);
  EXPECT_EQ(t.get().at("/").change_1h.value(), 300);
}

TEST(UsedHistory, TotalAddsUpTheDrivesThatHaveAValue) {
  used_history::change_total total;
  EXPECT_FALSE(total.change_1h);
  used_history::changes a;
  a.change_1h = 100;
  used_history::changes b;
  b.change_1h = -40;
  b.change_24h = 7;
  total.append(a);
  total.append(b);
  total.append(used_history::changes());
  ASSERT_TRUE(total.change_1h);
  EXPECT_EQ(total.change_1h.value(), 60);
  ASSERT_TRUE(total.change_24h);
  EXPECT_EQ(total.change_24h.value(), 7);
}

TEST(UsedHistory, HumanRendering) {
  EXPECT_EQ(used_history::format_change(boost::none), "unknown");
  EXPECT_EQ(used_history::format_change(boost::optional<long long>(0)), "0B");
  EXPECT_EQ(used_history::format_change(boost::optional<long long>(1536)), "+1.5KB");
  EXPECT_EQ(used_history::format_change(boost::optional<long long>(-1024LL * 1024)), "-1MB");
}
