// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <sampling_state.hpp>

using sampling::readiness;
using sampling::tracker;

TEST(SamplingTracker, NothingAttemptedIsWarmUp) {
  const tracker t;
  EXPECT_EQ(readiness::warming_up, t.get(false).state);
  EXPECT_TRUE(t.get(false).error.empty());
}

TEST(SamplingTracker, DataIsReadyWhateverHappenedSince) {
  tracker t;
  t.succeeded();
  t.failed("read failed");
  EXPECT_EQ(readiness::ready, t.get(true).state);
}

// The case warmup-state must never cover: the collector has been trying and
// the buffer is still empty.
TEST(SamplingTracker, AttemptedButEmptyIsFailedWithTheReason) {
  tracker t;
  t.failed("/proc/stat: permission denied");
  const sampling::status s = t.get(false);
  EXPECT_EQ(readiness::failed, s.state);
  EXPECT_EQ("/proc/stat: permission denied", s.error);
}

TEST(SamplingTracker, FailureWithoutAMessageStillSaysSomething) {
  tracker t;
  t.failed("");
  EXPECT_EQ("unknown error", t.get(false).error);
}

// A successful attempt that produced nothing (every interface filtered out)
// is not a warm-up either, and carries no error.
TEST(SamplingTracker, SucceededButEmptyIsFailedWithoutAnError) {
  tracker t;
  t.failed("transient");
  t.succeeded();
  const sampling::status s = t.get(false);
  EXPECT_EQ(readiness::failed, s.state);
  EXPECT_TRUE(s.error.empty());
}

// The first baseline of a delta source is still the warm-up.
TEST(SamplingTracker, FirstBaselineIsWarmUp) {
  tracker t;
  t.baseline();
  EXPECT_EQ(readiness::warming_up, t.get(false).state);
}

// After a run of failures, the read that only re-establishes a delta source's
// baseline is not a warm-up - the collector has tried - but no longer reports
// the old failure either.
TEST(SamplingTracker, BaselineAfterFailuresIsNotWarmUp) {
  tracker t;
  t.failed("/proc/stat: cannot open");
  t.failed("/proc/stat: cannot open");
  t.baseline();
  const sampling::status s = t.get(false);
  EXPECT_EQ(readiness::failed, s.state);
  EXPECT_TRUE(s.recovering);
  EXPECT_TRUE(s.error.empty());
  t.succeeded();
  EXPECT_EQ(readiness::ready, t.get(true).state);
}

// A source that worked and then died keeps old samples in its buffer; after a
// few failed ticks in a row those are no longer answered from.
TEST(SamplingTracker, PersistentFailureOverridesOldData) {
  tracker t;
  t.succeeded();
  for (long long i = 1; i < tracker::stale_after; ++i) {
    t.failed("/proc/stat: Permission denied");
    EXPECT_EQ(readiness::ready, t.get(true).state) << "a transient failure keeps the recent samples, after " << i;
  }
  t.failed("/proc/stat: Permission denied");
  const sampling::status s = t.get(true);
  EXPECT_EQ(readiness::failed, s.state);
  EXPECT_EQ("/proc/stat: Permission denied", s.error);
  t.succeeded();
  EXPECT_EQ(readiness::ready, t.get(true).state);
}
