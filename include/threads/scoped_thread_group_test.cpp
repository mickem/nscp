// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <boost/thread.hpp>
#include <stdexcept>
#include <string>
#include <threads/scoped_thread_group.hpp>

// --- Construction / destruction ---

TEST(scoped_thread_group, construct_and_destroy) {
  scoped_thread_group ht;
  EXPECT_EQ(ht.count(), 0u);
}

// --- create_thread ---

TEST(scoped_thread_group, create_single_thread) {
  scoped_thread_group ht;
  std::atomic<bool> ran{false};

  ht.create_thread([&]() { ran.store(true); });

  ht.wait_all();
  EXPECT_TRUE(ran.load());
}

TEST(scoped_thread_group, count_increments) {
  scoped_thread_group ht;
  boost::mutex mtx;
  boost::condition_variable cv;
  bool ready = false;

  // Launch a thread that blocks until we release it
  ht.create_thread([&]() {
    boost::mutex::scoped_lock lock(mtx);
    while (!ready) {
      cv.wait(lock);
    }
  });

  // Give the thread time to start
  boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
  EXPECT_EQ(ht.count(), 1u);

  // Release the thread
  {
    boost::mutex::scoped_lock lock(mtx);
    ready = true;
  }
  cv.notify_all();

  ht.wait_all();
}

TEST(scoped_thread_group, create_multiple_threads) {
  scoped_thread_group ht;
  std::atomic<int> counter{0};
  const int num_threads = 5;

  boost::mutex mtx;
  boost::condition_variable cv;
  bool go = false;

  for (int i = 0; i < num_threads; ++i) {
    ht.create_thread([&]() {
      // Wait until all threads are created
      {
        boost::mutex::scoped_lock lock(mtx);
        while (!go) {
          cv.wait(lock);
        }
      }
      counter.fetch_add(1);
    });
  }

  // Give threads time to start
  boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
  EXPECT_EQ(ht.count(), static_cast<size_t>(num_threads));

  // Release all threads
  {
    boost::mutex::scoped_lock lock(mtx);
    go = true;
  }
  cv.notify_all();

  ht.wait_all();
  EXPECT_EQ(counter.load(), num_threads);
}

// --- wait_all ---

TEST(scoped_thread_group, wait_all_blocks_until_complete) {
  scoped_thread_group ht;
  std::atomic<bool> finished{false};

  ht.create_thread([&]() {
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    finished.store(true);
  });

  ht.wait_all();
  EXPECT_TRUE(finished.load());
}

TEST(scoped_thread_group, wait_all_on_empty_is_noop) {
  scoped_thread_group ht;
  // Should not hang or crash
  ht.wait_all();
  EXPECT_EQ(ht.count(), 0u);
}

// --- interrupt_all ---

TEST(scoped_thread_group, interrupt_all) {
  scoped_thread_group ht;
  std::atomic<bool> interrupted{false};

  ht.create_thread([&]() {
    try {
      // Sleep indefinitely, relying on interruption to break out
      boost::this_thread::sleep_for(boost::chrono::seconds(60));
    } catch (boost::thread_interrupted&) {
      interrupted.store(true);
    }
  });

  // Give the thread time to start sleeping
  boost::this_thread::sleep_for(boost::chrono::milliseconds(50));

  ht.interrupt_all();
  ht.wait_all();
  EXPECT_TRUE(interrupted.load());
}

// --- thread count after completion ---

TEST(scoped_thread_group, count_decreases_after_thread_exits) {
  scoped_thread_group ht;

  ht.create_thread([&]() {
    // Finish immediately
  });

  // Wait a bit for the thread to finish and self-remove
  boost::this_thread::sleep_for(boost::chrono::milliseconds(200));

  EXPECT_EQ(ht.count(), 0u);
}

// --- destructor waits for threads ---

TEST(scoped_thread_group, destructor_waits_for_threads) {
  std::atomic<bool> finished{false};

  {
    scoped_thread_group ht;
    ht.create_thread([&]() {
      boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
      finished.store(true);
    });
    // ht goes out of scope - destructor should wait
  }

  EXPECT_TRUE(finished.load());
}

// --- exception in thread does not crash ---

TEST(scoped_thread_group, thread_exception_is_caught) {
  scoped_thread_group ht;
  std::atomic<bool> other_ran{false};

  ht.create_thread([&]() { throw std::runtime_error("test error"); });

  // Give time for the throwing thread to finish
  boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

  // Create another thread to verify the object is still functional
  ht.create_thread([&]() { other_ran.store(true); });

  ht.wait_all();
  EXPECT_TRUE(other_ran.load());
}

// --- error reporting ---

TEST(scoped_thread_group, error_reporter_reports_a_thread_death) {
  scoped_thread_group ht;
  std::string reported;
  ht.set_error_reporter([&](const std::string& message) { reported = message; });

  ht.create_thread([]() { throw std::runtime_error("test error"); }, "named worker");
  ht.wait_all();

  // The line is worded by the guard, not by the reporter: the pool used to be
  // wired to one that prefixed "Scheduler thread '...'" instead.
  EXPECT_EQ(reported, "Thread 'named worker': terminated by an uncaught exception: test error");
}

TEST(scoped_thread_group, a_worker_that_exits_normally_is_not_reported) {
  scoped_thread_group ht;
  std::atomic<int> reports{0};
  ht.set_error_reporter([&](const std::string&) { reports.fetch_add(1); });

  ht.create_thread([]() {});
  ht.wait_all();

  EXPECT_EQ(reports.load(), 0);
}

// --- wait_all_for ---

TEST(scoped_thread_group, wait_all_for_joins_threads_that_finish_in_time) {
  scoped_thread_group ht;
  std::atomic<int> done{0};
  for (int i = 0; i < 3; ++i) ht.create_thread([&]() { done.fetch_add(1); });
  EXPECT_EQ(ht.wait_all_for(boost::posix_time::seconds(5)), 0u);
  EXPECT_EQ(done.load(), 3);
}

TEST(scoped_thread_group, wait_all_for_gives_up_on_a_hung_thread_which_may_outlive_the_group) {
  boost::mutex mtx;
  boost::condition_variable cv;
  bool release = false;
  std::atomic<bool> finished{false};
  {
    scoped_thread_group ht;
    ht.create_thread([&]() {});
    ht.create_thread([&]() {
      boost::mutex::scoped_lock lock(mtx);
      while (!release) cv.wait(lock);
      finished = true;
    });
    const auto before = std::chrono::steady_clock::now();
    EXPECT_EQ(ht.wait_all_for(boost::posix_time::milliseconds(200)), 1u);
    EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::seconds(3));
    // The destructor must not join the thread it gave up on again.
  }
  // The group is gone; the thread it abandoned still finishes against the
  // state it shares with it rather than the destroyed group.
  {
    boost::mutex::scoped_lock lock(mtx);
    release = true;
  }
  cv.notify_all();
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!finished && std::chrono::steady_clock::now() < deadline) boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
  EXPECT_TRUE(finished.load());
  boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
}
