// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "realtime_thread.hpp"

TEST(pdh_thread, stop_does_not_wait_out_the_sampling_interval) {
  // The collector samples once a second. stop() - every module unload and
  // every settings reload - used to wait for the rest of that second before
  // the join returned; it now wakes the wait. Each attempt stops a collector
  // 300 ms into its second tick's wait, where the old sleep still had about
  // 700 ms to go. The best of three is taken so that one stall of a loaded
  // runner (or ThreadSanitizer) cannot fail the case; the old code is slow
  // on every attempt, not just one.
  long long best_ms = -1;
  for (int attempt = 0; attempt < 3 && (best_ms < 0 || best_ms >= 500); ++attempt) {
    pdh_thread collector;
    collector.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1300));

    const auto before = std::chrono::steady_clock::now();
    collector.stop();
    const long long took_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before).count();
    if (best_ms < 0 || took_ms < best_ms) best_ms = took_ms;
  }

  EXPECT_LT(best_ms, 500) << "stop() took at least " << best_ms << " ms on every attempt";
}

TEST(pdh_thread, stop_is_idempotent_and_safe_before_start) {
  pdh_thread never_started;
  EXPECT_TRUE(never_started.stop());

  pdh_thread collector;
  collector.start();
  EXPECT_TRUE(collector.stop());
  EXPECT_TRUE(collector.stop());
}
