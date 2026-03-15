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

#include <boost/thread.hpp>
#include <string>
#include <threads/concurrent_queue.hpp>
#include <vector>

// --- empty ---

TEST(concurrent_queue, empty_on_construction) {
  concurrent_queue<int> q;
  EXPECT_TRUE(q.empty());
}

TEST(concurrent_queue, not_empty_after_push) {
  concurrent_queue<int> q;
  q.push(1);
  EXPECT_FALSE(q.empty());
}

// --- try_pop ---

TEST(concurrent_queue, try_pop_empty_returns_false) {
  concurrent_queue<int> q;
  int val = 0;
  EXPECT_FALSE(q.try_pop(val));
}

TEST(concurrent_queue, try_pop_single_element) {
  concurrent_queue<int> q;
  q.push(42);

  int val = 0;
  EXPECT_TRUE(q.try_pop(val));
  EXPECT_EQ(val, 42);
  EXPECT_TRUE(q.empty());
}

TEST(concurrent_queue, try_pop_fifo_order) {
  concurrent_queue<int> q;
  q.push(1);
  q.push(2);
  q.push(3);

  int val = 0;
  EXPECT_TRUE(q.try_pop(val));
  EXPECT_EQ(val, 1);
  EXPECT_TRUE(q.try_pop(val));
  EXPECT_EQ(val, 2);
  EXPECT_TRUE(q.try_pop(val));
  EXPECT_EQ(val, 3);
  EXPECT_FALSE(q.try_pop(val));
}

// --- wait_and_pop ---

TEST(concurrent_queue, wait_and_pop_available_element) {
  concurrent_queue<int> q;
  q.push(99);

  int val = 0;
  q.wait_and_pop(val);
  EXPECT_EQ(val, 99);
}

TEST(concurrent_queue, wait_and_pop_blocks_until_push) {
  concurrent_queue<int> q;
  int result = 0;

  boost::thread consumer([&]() { q.wait_and_pop(result); });

  // Small delay so consumer thread enters wait
  boost::this_thread::sleep_for(boost::chrono::milliseconds(50));

  q.push(77);
  consumer.join();

  EXPECT_EQ(result, 77);
}

// --- string values ---

TEST(concurrent_queue, works_with_strings) {
  concurrent_queue<std::string> q;
  q.push("hello");
  q.push("world");

  std::string val;
  EXPECT_TRUE(q.try_pop(val));
  EXPECT_EQ(val, "hello");
  EXPECT_TRUE(q.try_pop(val));
  EXPECT_EQ(val, "world");
}

// --- thread safety (smoke test) ---

TEST(concurrent_queue, concurrent_push_try_pop) {
  concurrent_queue<int> q;
  const int count = 1000;

  boost::thread producer([&]() {
    for (int i = 0; i < count; ++i) {
      q.push(i);
    }
  });

  std::vector<int> results;
  boost::thread consumer([&]() {
    int consumed = 0;
    while (consumed < count) {
      int val;
      if (q.try_pop(val)) {
        results.push_back(val);
        ++consumed;
      } else {
        boost::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(static_cast<int>(results.size()), count);
  // Single producer + single consumer: FIFO order preserved
  for (int i = 0; i < count; ++i) {
    EXPECT_EQ(results[i], i);
  }
}

TEST(concurrent_queue, concurrent_push_wait_and_pop) {
  concurrent_queue<int> q;
  const int count = 100;

  boost::thread producer([&]() {
    for (int i = 0; i < count; ++i) {
      q.push(i);
      boost::this_thread::sleep_for(boost::chrono::microseconds(100));
    }
  });

  std::vector<int> results;
  boost::thread consumer([&]() {
    for (int i = 0; i < count; ++i) {
      int val;
      q.wait_and_pop(val);
      results.push_back(val);
    }
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(static_cast<int>(results.size()), count);
  for (int i = 0; i < count; ++i) {
    EXPECT_EQ(results[i], i);
  }
}
