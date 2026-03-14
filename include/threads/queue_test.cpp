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

#include <threads/queue.hpp>

#include <queue>
#include <string>

// Use std::queue (FIFO) as the underlying container for simpler test reasoning
using int_queue = nscp_thread::safe_queue<int, std::queue<int>>;
using string_queue = nscp_thread::safe_queue<std::string, std::queue<std::string>>;

// --- empty ---

TEST(safe_queue, empty_on_construction) {
  int_queue q;
  EXPECT_TRUE(q.empty());
}

TEST(safe_queue, not_empty_after_push) {
  int_queue q;
  EXPECT_TRUE(q.push(42));
  EXPECT_FALSE(q.empty());
}

// --- push / pop ---

TEST(safe_queue, push_and_pop_single) {
  int_queue q;
  EXPECT_TRUE(q.push(7));

  auto val = q.pop();
  ASSERT_TRUE(val.is_initialized());
  EXPECT_EQ(*val, 7);
}

TEST(safe_queue, pop_empty_returns_none) {
  int_queue q;
  auto val = q.pop();
  EXPECT_FALSE(val.is_initialized());
}

TEST(safe_queue, push_pop_fifo_order) {
  int_queue q;
  q.push(1);
  q.push(2);
  q.push(3);

  EXPECT_EQ(*q.pop(), 1);
  EXPECT_EQ(*q.pop(), 2);
  EXPECT_EQ(*q.pop(), 3);
  EXPECT_FALSE(q.pop().is_initialized());
}

// --- top ---

TEST(safe_queue, top_returns_front_without_removing) {
  int_queue q;
  q.push(10);
  q.push(20);

  auto val = q.top();
  ASSERT_TRUE(val.is_initialized());
  EXPECT_EQ(*val, 10);

  // top should not remove the element
  EXPECT_EQ(q.size(), 2u);
}

TEST(safe_queue, top_empty_returns_none) {
  int_queue q;
  auto val = q.top();
  EXPECT_FALSE(val.is_initialized());
}

// --- size ---

TEST(safe_queue, size_zero_on_construction) {
  int_queue q;
  EXPECT_EQ(q.size(), 0u);
}

TEST(safe_queue, size_increases_after_push) {
  int_queue q;
  q.push(1);
  EXPECT_EQ(q.size(), 1u);
  q.push(2);
  EXPECT_EQ(q.size(), 2u);
}

TEST(safe_queue, size_decreases_after_pop) {
  int_queue q;
  q.push(1);
  q.push(2);
  q.pop();
  EXPECT_EQ(q.size(), 1u);
}

TEST(safe_queue, empty_after_popping_all) {
  int_queue q;
  q.push(1);
  q.pop();
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.size(), 0u);
}

// --- string values ---

TEST(safe_queue, works_with_strings) {
  string_queue q;
  q.push("hello");
  q.push("world");

  EXPECT_EQ(*q.top(), "hello");
  EXPECT_EQ(*q.pop(), "hello");
  EXPECT_EQ(*q.pop(), "world");
  EXPECT_FALSE(q.pop().is_initialized());
}

// --- thread safety (basic smoke test) ---

TEST(safe_queue, concurrent_push_pop) {
  int_queue q;
  const int count = 1000;

  boost::thread producer([&]() {
    for (int i = 0; i < count; ++i) {
      while (!q.push(i)) {
        // retry on timeout
      }
    }
  });

  std::vector<int> results;
  boost::thread consumer([&]() {
    int consumed = 0;
    while (consumed < count) {
      auto val = q.pop();
      if (val.is_initialized()) {
        results.push_back(*val);
        ++consumed;
      }
    }
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(static_cast<int>(results.size()), count);
  // With a single producer and single consumer using FIFO queue, order is preserved
  for (int i = 0; i < count; ++i) {
    EXPECT_EQ(results[i], i);
  }
}

