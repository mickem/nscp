// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <limits>
#include <net/web_server_limits.hpp>
#include <string>

namespace {

std::size_t parse_ok(const std::string &value) {
  std::size_t bytes = 0;
  std::string error;
  EXPECT_TRUE(net::parse_request_size(value, bytes, error)) << "'" << value << "' refused: " << error;
  return bytes;
}

std::string parse_error(const std::string &value) {
  std::size_t bytes = 0;
  std::string error;
  EXPECT_FALSE(net::parse_request_size(value, bytes, error)) << "'" << value << "' accepted as " << bytes;
  return error;
}

}  // namespace

TEST(WebServerLimits, PlainByteCount) {
  EXPECT_EQ(parse_ok("1048576"), 1048576u);
  EXPECT_EQ(parse_ok("1"), 1u);
  EXPECT_EQ(parse_ok("512b"), 512u);
  EXPECT_EQ(parse_ok("512B"), 512u);
}

TEST(WebServerLimits, UnitsInEverySpelling) {
  EXPECT_EQ(parse_ok("4k"), 4096u);
  EXPECT_EQ(parse_ok("4K"), 4096u);
  EXPECT_EQ(parse_ok("4kb"), 4096u);
  EXPECT_EQ(parse_ok("4KiB"), 4096u);
  EXPECT_EQ(parse_ok("3m"), 3u * 1024u * 1024u);
  EXPECT_EQ(parse_ok("3MB"), 3u * 1024u * 1024u);
  EXPECT_EQ(parse_ok("3MiB"), 3u * 1024u * 1024u);
  EXPECT_EQ(parse_ok("1g"), 1024u * 1024u * 1024u);
  EXPECT_EQ(parse_ok("1GiB"), 1024u * 1024u * 1024u);
}

TEST(WebServerLimits, BlanksAroundAndBetween) {
  EXPECT_EQ(parse_ok("  4k  "), 4096u);
  EXPECT_EQ(parse_ok("4 KiB"), 4096u);
  EXPECT_EQ(parse_ok("\t8 m"), 8u * 1024u * 1024u);
}

TEST(WebServerLimits, TheDefaultIsWhatTheServersAccept) {
  // Mongoose::Server::kDefaultBodyLimit, spelled out rather than included:
  // this header has no business pulling the HTTP layer into the net tests.
  EXPECT_EQ(parse_ok(net::kDefaultWebMaxRequestSize), 3u * 1024u * 1024u);
}

TEST(WebServerLimits, RefusesWhatItCannotMean) {
  EXPECT_NE(parse_error("").find("no value"), std::string::npos);
  EXPECT_NE(parse_error("   ").find("no value"), std::string::npos);
  EXPECT_NE(parse_error("lots").find("expected a number"), std::string::npos);
  EXPECT_NE(parse_error("-1").find("expected a number"), std::string::npos);
  EXPECT_NE(parse_error("4x").find("unknown unit 'x'"), std::string::npos);
  EXPECT_NE(parse_error("4 kilobytes").find("unknown unit"), std::string::npos);
  EXPECT_NE(parse_error("0").find("more than zero"), std::string::npos);
  EXPECT_NE(parse_error("0k").find("more than zero"), std::string::npos);
}

TEST(WebServerLimits, RefusesWhatDoesNotFit) {
  EXPECT_NE(parse_error("99999999999999999999999").find("too large"), std::string::npos);
  // 2^64 - 1 is a valid number that no multiplier leaves in range.
  EXPECT_NE(parse_error("18446744073709551615k").find("larger than this platform"), std::string::npos);
  if constexpr (sizeof(std::size_t) == 4) {
    EXPECT_NE(parse_error("4g").find("larger than this platform"), std::string::npos);
  } else {
    EXPECT_EQ(parse_ok("4g"), 4ull * 1024ull * 1024ull * 1024ull);
  }
}
