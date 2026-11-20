// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_netstring.hpp"

#include <gtest/gtest.h>

using namespace icinga_agent;

TEST(IcingaAgentNetstring, FrameUsesTheByteLength) {
  EXPECT_EQ(frame("abc"), "3:abc,");
  EXPECT_EQ(frame("\xc3\xa5"), "2:\xc3\xa5,");  // one character, two bytes
  EXPECT_EQ(frame(""), "0:,");
}

TEST(IcingaAgentNetstring, DecodesSeveralFramesInOneRead) {
  netstring_reader r;
  const std::string wire = frame("a") + frame("bb") + frame("");
  const auto out = r.feed(wire.data(), wire.size());
  ASSERT_EQ(out.size(), 3u);
  EXPECT_EQ(out[0], "a");
  EXPECT_EQ(out[1], "bb");
  EXPECT_EQ(out[2], "");
  EXPECT_EQ(r.buffered(), 0u);
}

TEST(IcingaAgentNetstring, KeepsAPartialFrameUntilItCompletes) {
  netstring_reader r;
  const std::string wire = frame(R"({"a":1})");
  std::vector<std::string> out;
  for (const char c : wire) {
    // One byte at a time: nothing until the terminating ','.
    EXPECT_TRUE(out.empty());
    out = r.feed(&c, 1);
  }
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0], R"({"a":1})");
}

TEST(IcingaAgentNetstring, ToleratesAStrayComma) {
  netstring_reader r;
  const std::string wire = "," + frame("x");
  const auto out = r.feed(wire.data(), wire.size());
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0], "x");
}

TEST(IcingaAgentNetstring, RejectsANonNumericPrefix) {
  netstring_reader r;
  EXPECT_THROW(r.feed("x:abc,", 6), netstring_error);
}

TEST(IcingaAgentNetstring, RejectsAnEmptyPrefix) {
  netstring_reader r;
  EXPECT_THROW(r.feed(":abc,", 5), netstring_error);
}

TEST(IcingaAgentNetstring, RejectsAPrefixThatNeverEnds) {
  netstring_reader r;
  const std::string digits(netstring_reader::kMaxPrefixDigits + 1, '1');
  EXPECT_THROW(r.feed(digits.data(), digits.size()), netstring_error);
}

TEST(IcingaAgentNetstring, RejectsAMessageOverTheLimit) {
  netstring_reader r(10);
  EXPECT_THROW(r.feed("11:", 3), netstring_error);
  netstring_reader exact(10);
  const std::string wire = frame("0123456789");
  EXPECT_EQ(exact.feed(wire.data(), wire.size()).size(), 1u);
}

TEST(IcingaAgentNetstring, RejectsAMissingTerminator) {
  netstring_reader r;
  EXPECT_THROW(r.feed("3:abcX", 6), netstring_error);
}
