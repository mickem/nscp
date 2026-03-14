/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <boost/thread.hpp>
#include <threads/has-threads.hpp>

// --- Construction / destruction ---

TEST(has_threads, construct_and_destroy) {
  has_threads ht;
  EXPECT_EQ(ht.threadCount(), 0u);
}

// --- createThread ---

TEST(has_threads, create_single_thread) {
  has_threads ht;
  std::atomic<bool> ran{false};

  ht.createThread([&]() { ran.store(true); });

  ht.waitForThreads();
  EXPECT_TRUE(ran.load());
}

TEST(has_threads, thread_count_increments) {
  has_threads ht;
  boost::mutex mtx;
  boost::condition_variable cv;
  bool ready = false;

  // Launch a thread that blocks until we release it
  ht.createThread([&]() {
    boost::mutex::scoped_lock lock(mtx);
    while (!ready) {
      cv.wait(lock);
    }
  });

  // Give the thread time to start
  boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
  EXPECT_EQ(ht.threadCount(), 1u);

  // Release the thread
  {
    boost::mutex::scoped_lock lock(mtx);
    ready = true;
  }
  cv.notify_all();

  ht.waitForThreads();
}

TEST(has_threads, create_multiple_threads) {
  has_threads ht;
  std::atomic<int> counter{0};
  const int num_threads = 5;

  boost::mutex mtx;
  boost::condition_variable cv;
  bool go = false;

  for (int i = 0; i < num_threads; ++i) {
    ht.createThread([&]() {
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
  EXPECT_EQ(ht.threadCount(), static_cast<size_t>(num_threads));

  // Release all threads
  {
    boost::mutex::scoped_lock lock(mtx);
    go = true;
  }
  cv.notify_all();

  ht.waitForThreads();
  EXPECT_EQ(counter.load(), num_threads);
}

// --- waitForThreads ---

TEST(has_threads, wait_for_threads_blocks_until_complete) {
  has_threads ht;
  std::atomic<bool> finished{false};

  ht.createThread([&]() {
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    finished.store(true);
  });

  ht.waitForThreads();
  EXPECT_TRUE(finished.load());
}

TEST(has_threads, wait_for_threads_on_empty_is_noop) {
  has_threads ht;
  // Should not hang or crash
  ht.waitForThreads();
  EXPECT_EQ(ht.threadCount(), 0u);
}

// --- interruptThreads ---

TEST(has_threads, interrupt_threads) {
  has_threads ht;
  std::atomic<bool> interrupted{false};

  ht.createThread([&]() {
    try {
      // Sleep indefinitely, relying on interruption to break out
      boost::this_thread::sleep_for(boost::chrono::seconds(60));
    } catch (boost::thread_interrupted&) {
      interrupted.store(true);
    }
  });

  // Give the thread time to start sleeping
  boost::this_thread::sleep_for(boost::chrono::milliseconds(50));

  ht.interruptThreads();
  ht.waitForThreads();
  EXPECT_TRUE(interrupted.load());
}

// --- thread count after completion ---

TEST(has_threads, thread_count_decreases_after_thread_exits) {
  has_threads ht;

  ht.createThread([&]() {
    // Finish immediately
  });

  // Wait a bit for the thread to finish and self-remove
  boost::this_thread::sleep_for(boost::chrono::milliseconds(200));

  EXPECT_EQ(ht.threadCount(), 0u);
}

// --- destructor waits for threads ---

TEST(has_threads, destructor_waits_for_threads) {
  std::atomic<bool> finished{false};

  {
    has_threads ht;
    ht.createThread([&]() {
      boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
      finished.store(true);
    });
    // ht goes out of scope - destructor should wait
  }

  EXPECT_TRUE(finished.load());
}

// --- exception in thread does not crash ---

TEST(has_threads, thread_exception_is_caught) {
  has_threads ht;
  std::atomic<bool> other_ran{false};

  ht.createThread([&]() { throw std::runtime_error("test error"); });

  // Give time for the throwing thread to finish
  boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

  // Create another thread to verify the object is still functional
  ht.createThread([&]() { other_ran.store(true); });

  ht.waitForThreads();
  EXPECT_TRUE(other_ran.load());
}
