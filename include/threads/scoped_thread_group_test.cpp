/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <boost/thread.hpp>
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
