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

#include <algorithms/pair_hash.h>
#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <utility>

// ============================================================================
// Tests for pair_hash: basic determinism
// ============================================================================

TEST(pair_hash, same_int_pair_gives_same_hash) {
  pair_hash h;
  auto p = std::make_pair(1, 2);
  EXPECT_EQ(h(p), h(p));
}

TEST(pair_hash, same_string_pair_gives_same_hash) {
  pair_hash h;
  auto p = std::make_pair(std::string("foo"), std::string("bar"));
  EXPECT_EQ(h(p), h(p));
}

// ============================================================================
// Tests for pair_hash: commutativity is NOT expected —
// pair_hash(a, b) is generally different from pair_hash(b, a)
// ============================================================================

TEST(pair_hash, reversed_pair_differs) {
  pair_hash h;
  auto p1 = std::make_pair(1, 2);
  auto p2 = std::make_pair(2, 1);
  // The XOR formula makes p(1,2) and p(2,1) produce the same combined hash
  // components but in swapped roles — the mixing formula is asymmetric so they
  // should differ for most inputs, but we just verify both produce valid values.
  EXPECT_NE(h(p1), h(p2));
}

// ============================================================================
// Tests for pair_hash: different pairs produce different hashes
// ============================================================================

TEST(pair_hash, different_int_pairs_differ) {
  pair_hash h;
  EXPECT_NE(h(std::make_pair(0, 0)), h(std::make_pair(0, 1)));
  EXPECT_NE(h(std::make_pair(0, 0)), h(std::make_pair(1, 0)));
  EXPECT_NE(h(std::make_pair(1, 1)), h(std::make_pair(1, 2)));
}

TEST(pair_hash, different_string_pairs_differ) {
  pair_hash h;
  auto p1 = std::make_pair(std::string("a"), std::string("b"));
  auto p2 = std::make_pair(std::string("a"), std::string("c"));
  auto p3 = std::make_pair(std::string("x"), std::string("b"));
  EXPECT_NE(h(p1), h(p2));
  EXPECT_NE(h(p1), h(p3));
}

// ============================================================================
// Tests for pair_hash: zero/empty pairs
// ============================================================================

TEST(pair_hash, zero_int_pair) {
  pair_hash h;
  auto p = std::make_pair(0, 0);
  EXPECT_NO_THROW(h(p));
}

TEST(pair_hash, empty_string_pair) {
  pair_hash h;
  auto p = std::make_pair(std::string(""), std::string(""));
  EXPECT_NO_THROW(h(p));
}

// ============================================================================
// Tests for pair_hash: usable in unordered_map
// ============================================================================

TEST(pair_hash, usable_in_unordered_map_int) {
  std::unordered_map<std::pair<int, int>, std::string, pair_hash> m;
  m[{1, 2}] = "one-two";
  m[{3, 4}] = "three-four";
  EXPECT_EQ(m.at({1, 2}), "one-two");
  EXPECT_EQ(m.at({3, 4}), "three-four");
  EXPECT_EQ(m.count({5, 6}), 0u);
}

TEST(pair_hash, usable_in_unordered_map_string) {
  std::unordered_map<std::pair<std::string, std::string>, int, pair_hash> m;
  m[{"host", "port"}] = 42;
  m[{"user", "pass"}] = 99;
  EXPECT_EQ(m.at({"host", "port"}), 42);
  EXPECT_EQ(m.at({"user", "pass"}), 99);
  EXPECT_EQ(m.count({"missing", "key"}), 0u);
}

TEST(pair_hash, many_insertions_no_collision_failures) {
  std::unordered_map<std::pair<int, int>, int, pair_hash> m;
  for (int i = 0; i < 50; ++i) {
    for (int j = 0; j < 50; ++j) {
      m[{i, j}] = i * 100 + j;
    }
  }
  EXPECT_EQ(m.size(), 2500u);
  for (int i = 0; i < 50; ++i) {
    for (int j = 0; j < 50; ++j) {
      EXPECT_EQ(m.at({i, j}), i * 100 + j);
    }
  }
}
