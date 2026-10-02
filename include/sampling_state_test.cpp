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

// After a run of failures, the read that only re-establishes a delta source's
// baseline puts it back into warm-up until the next read produces a sample.
TEST(SamplingTracker, RestartedAfterFailuresIsWarmUpAgain) {
  tracker t;
  t.failed("/proc/stat: cannot open");
  t.restarted();
  EXPECT_EQ(readiness::warming_up, t.get(false).state);
  t.succeeded();
  EXPECT_EQ(readiness::ready, t.get(true).state);
}
