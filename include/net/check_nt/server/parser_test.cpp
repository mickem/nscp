// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <net/check_nt/packet.hpp>
#include <net/check_nt/server/parser.hpp>
#include <string>
#include <vector>

namespace {

// Feed a whole string to the parser in one digest() call, the way one recv()
// chunk arrives, and return (complete, consumed count).
std::pair<bool, std::size_t> digest_chunk(check_nt::server::parser &parser, const std::string &chunk) {
  std::vector<char> buf(chunk.begin(), chunk.end());
  char *begin = buf.data();
  char *end = begin + buf.size();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);
  return std::make_pair(complete, static_cast<std::size_t>(begin - buf.data()));
}

}  // namespace

// The real nagios-plugins check_nt sends `<password>&<cmd>&<args>` with NO
// trailing newline and then waits for the response: end-of-chunk must be
// end-of-request or every real client hangs until its socket timeout.
TEST(CheckNtParser, RequestWithoutNewlineCompletesAtEndOfChunk) {
  check_nt::server::parser parser;

  const std::string request = "password&1";
  auto r = digest_chunk(parser, request);
  EXPECT_TRUE(r.first);
  EXPECT_EQ(r.second, request.size());

  EXPECT_EQ(parser.parse().get_payload(), request);
}

TEST(CheckNtParser, RequestWithNewlineCompletesAtTheNewline) {
  check_nt::server::parser parser;

  auto r = digest_chunk(parser, "password&3\ntrailing");
  EXPECT_TRUE(r.first);
  // The newline is consumed with the request; the pipelined bytes after it
  // are left for the next digest() round.
  EXPECT_EQ(r.second, std::string("password&3\n").size());

  EXPECT_EQ(parser.parse().get_payload(), "password&3\n");
}

TEST(CheckNtParser, EmptyChunkIsNotARequest) {
  check_nt::server::parser parser;

  auto r = digest_chunk(parser, "");
  EXPECT_FALSE(r.first);
  EXPECT_EQ(r.second, 0u);
}

TEST(CheckNtParser, OversizedLineIsCappedAndSurfacedAsComplete) {
  check_nt::server::parser parser;

  const std::string flood(8000, 'x');  // no newline, larger than the 4 KiB cap
  auto r = digest_chunk(parser, flood);
  EXPECT_TRUE(r.first);
  EXPECT_LT(r.second, flood.size());

  // The buffered (capped) prefix is handed to the handler as-is; the
  // connection is then closed by the protocol layer.
  EXPECT_EQ(parser.parse().get_payload().size(), 4096u);
}
