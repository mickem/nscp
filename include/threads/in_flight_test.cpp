// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "in_flight.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

using threads::in_flight;

namespace {

// A region another thread holds open until told to let go.
struct held_region {
  in_flight &tracker;
  std::mutex mu;
  std::condition_variable cv;
  bool entered = false;
  bool released = false;
  std::thread thread;

  explicit held_region(in_flight &t) : tracker(t) {
    thread = std::thread([this]() {
      in_flight::guard g(tracker);
      g.enter();
      {
        std::unique_lock<std::mutex> lock(mu);
        entered = true;
        cv.notify_all();
        cv.wait(lock, [this]() { return released; });
      }
    });
    std::unique_lock<std::mutex> lock(mu);
    cv.wait(lock, [this]() { return entered; });
  }
  void release() {
    {
      std::lock_guard<std::mutex> lock(mu);
      released = true;
    }
    cv.notify_all();
  }
  ~held_region() {
    release();
    thread.join();
  }
};

}  // namespace

TEST(InFlight, NothingInsideReturnsAtOnce) {
  in_flight tracker;
  const auto started = std::chrono::steady_clock::now();
  EXPECT_TRUE(tracker.wait_for_others_before(tracker.cutoff(), std::chrono::seconds(5)));
  EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(1));
}

TEST(InFlight, GuardEntersOnceAndNestedGuardsLeaveInnermostFirst) {
  in_flight tracker;
  {
    in_flight::guard g(tracker);
    EXPECT_FALSE(g.entered());
    EXPECT_FALSE(tracker.on_this_thread());
    g.enter();
    g.enter();  // idempotent
    EXPECT_TRUE(g.entered());
    EXPECT_TRUE(tracker.on_this_thread());
    {
      in_flight::guard nested(tracker);
      nested.enter();
      EXPECT_TRUE(tracker.on_this_thread());
    }
    EXPECT_TRUE(tracker.on_this_thread());
    // The outer entry is still there after the nested one left: a waiter on
    // another thread still sees it.
    const std::uint64_t cutoff = tracker.cutoff();
    std::atomic<bool> timed_out{false};
    std::thread waiter([&]() { timed_out = !tracker.wait_for_others_before(cutoff, std::chrono::milliseconds(100)); });
    waiter.join();
    EXPECT_TRUE(timed_out.load());
  }
  // And gone once the outer guard went.
  EXPECT_FALSE(tracker.on_this_thread());
  EXPECT_TRUE(tracker.wait_for_others_before(tracker.cutoff(), std::chrono::milliseconds(100)));
}

TEST(InFlight, OwnEntriesAreNotWaitedFor) {
  in_flight tracker;
  in_flight::guard g(tracker);
  g.enter();
  const auto started = std::chrono::steady_clock::now();
  EXPECT_TRUE(tracker.wait_for_others_before(tracker.cutoff(), std::chrono::seconds(5)));
  EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(1));
}

TEST(InFlight, WaitsForAnotherThreadsEntryBelowTheCutoff) {
  in_flight tracker;
  held_region held(tracker);
  const std::uint64_t cutoff = tracker.cutoff();

  std::atomic<bool> returned{false};
  std::thread waiter([&]() {
    EXPECT_TRUE(tracker.wait_for_others_before(cutoff, std::chrono::seconds(5)));
    returned = true;
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_FALSE(returned.load());
  held.release();
  waiter.join();
  EXPECT_TRUE(returned.load());
}

TEST(InFlight, EntriesAboveTheCutoffAreNotWaitedFor) {
  in_flight tracker;
  const std::uint64_t cutoff = tracker.cutoff();
  // Entered after the cutoff was taken: cannot hold what the waiter removed.
  held_region later(tracker);
  const auto started = std::chrono::steady_clock::now();
  EXPECT_TRUE(tracker.wait_for_others_before(cutoff, std::chrono::seconds(5)));
  EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(1));
}

TEST(InFlight, TimesOutOnAnEntryThatNeverLeaves) {
  in_flight tracker;
  held_region held(tracker);
  const auto started = std::chrono::steady_clock::now();
  EXPECT_FALSE(tracker.wait_for_others_before(tracker.cutoff(), std::chrono::milliseconds(200)));
  const auto elapsed = std::chrono::steady_clock::now() - started;
  EXPECT_GE(elapsed, std::chrono::milliseconds(200));
  EXPECT_LT(elapsed, std::chrono::seconds(5));
}
