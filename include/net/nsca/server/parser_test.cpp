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

#include <net/nsca/nsca_packet.hpp>
#include <net/nsca/server/parser.hpp>
#include <string>

// =============================================================================
// parser — initial state
// =============================================================================

TEST(NscaParser, InitialStateEmpty) {
  nsca::server::parser parser(512);
  EXPECT_EQ(parser.size(), 0u);
  EXPECT_TRUE(parser.get_buffer().empty());
}

// =============================================================================
// parser — digest complete packet
// =============================================================================

TEST(NscaParser, DigestCompletePacket) {
  const unsigned int payload = 512;
  nsca::server::parser parser(payload);

  // Build a valid NSCA packet
  nsca::packet original(std::string("testhost"), payload);
  original.service = "check_cpu";
  original.code = 0;
  original.result = "OK";

  std::string buffer(original.get_packet_length(), '\0');
  original.get_buffer(buffer);

  // Feed entire buffer in one call
  auto begin = buffer.begin();
  auto end = buffer.end();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);

  EXPECT_TRUE(complete);
  EXPECT_EQ(parser.size(), nsca::length::get_packet_length(payload));
}

// =============================================================================
// parser — digest incremental
// =============================================================================

TEST(NscaParser, DigestIncrementally) {
  const unsigned int payload = 512;
  nsca::server::parser parser(payload);

  nsca::packet original(std::string("host"), payload);
  original.service = "svc";
  original.code = 1;
  original.result = "WARNING";

  std::string buffer(original.get_packet_length(), '\0');
  original.get_buffer(buffer);

  // Feed one byte at a time
  bool complete = false;
  std::size_t offset = 0;
  while (offset < buffer.size() && !complete) {
    auto begin = buffer.begin() + offset;
    auto end = begin + 1;
    boost::tie(complete, begin) = parser.digest(begin, end);
    offset++;
  }

  EXPECT_TRUE(complete);
}

// =============================================================================
// parser — digest in chunks
// =============================================================================

TEST(NscaParser, DigestInChunks) {
  const unsigned int payload = 512;
  nsca::server::parser parser(payload);

  nsca::packet original(std::string("host"), payload);
  original.service = "svc";
  original.code = 2;
  original.result = "CRITICAL";

  std::string buffer(original.get_packet_length(), '\0');
  original.get_buffer(buffer);

  const std::size_t chunk_size = 64;
  bool complete = false;
  std::size_t offset = 0;
  while (offset < buffer.size() && !complete) {
    std::size_t remaining = buffer.size() - offset;
    std::size_t this_chunk = std::min(chunk_size, remaining);
    auto begin = buffer.begin() + offset;
    auto end = begin + this_chunk;
    boost::tie(complete, begin) = parser.digest(begin, end);
    offset += this_chunk;
  }

  EXPECT_TRUE(complete);
}

// =============================================================================
// parser — parse after digest
// =============================================================================

TEST(NscaParser, ParseAfterDigest) {
  const unsigned int payload = 512;
  nsca::server::parser parser(payload);

  nsca::packet original(std::string("myhost"), payload);
  original.service = "check_disk";
  original.code = 0;
  original.result = "OK - plenty of space";

  std::string buffer(original.get_packet_length(), '\0');
  original.get_buffer(buffer);

  auto begin = buffer.begin();
  auto end = buffer.end();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);
  ASSERT_TRUE(complete);

  nsca::packet parsed = parser.parse();
  EXPECT_EQ(parsed.host, "myhost");
  EXPECT_EQ(parsed.service, "check_disk");
  EXPECT_EQ(parsed.code, 0u);
  EXPECT_EQ(parsed.result, "OK - plenty of space");
}

// =============================================================================
// parser — reset clears state
// =============================================================================

TEST(NscaParser, ResetClearsBuffer) {
  const unsigned int payload = 512;
  nsca::server::parser parser(payload);

  nsca::packet original(std::string("host"), payload);
  original.service = "svc";
  original.code = 0;
  original.result = "OK";

  std::string buffer(original.get_packet_length(), '\0');
  original.get_buffer(buffer);

  // Partially feed data
  auto begin = buffer.begin();
  auto end = buffer.begin() + 100;
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);
  EXPECT_FALSE(complete);
  EXPECT_GT(parser.size(), 0u);

  parser.reset();
  EXPECT_EQ(parser.size(), 0u);
  EXPECT_TRUE(parser.get_buffer().empty());
}

// =============================================================================
// parser — parse clears buffer for reuse
// =============================================================================

TEST(NscaParser, ParseClearsBufferForReuse) {
  const unsigned int payload = 512;
  nsca::server::parser parser(payload);

  nsca::packet original(std::string("host"), payload);
  original.service = "svc";
  original.code = 0;
  original.result = "OK";

  std::string buffer(original.get_packet_length(), '\0');
  original.get_buffer(buffer);

  auto begin = buffer.begin();
  auto end = buffer.end();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);
  ASSERT_TRUE(complete);

  parser.parse();

  // After parse, internal buffer is cleared
  EXPECT_EQ(parser.size(), 0u);

  // Can digest a new packet
  nsca::packet second(std::string("host2"), payload);
  second.service = "svc2";
  second.code = 1;
  second.result = "WARNING";

  std::string buffer2(second.get_packet_length(), '\0');
  second.get_buffer(buffer2);

  begin = buffer2.begin();
  end = buffer2.end();
  boost::tie(complete, begin) = parser.digest(begin, end);
  EXPECT_TRUE(complete);

  nsca::packet parsed2 = parser.parse();
  EXPECT_EQ(parsed2.host, "host2");
  EXPECT_EQ(parsed2.service, "svc2");
}

// =============================================================================
// parser — incomplete digest returns false
// =============================================================================

TEST(NscaParser, IncompleteDigestReturnsFalse) {
  const unsigned int payload = 512;
  nsca::server::parser parser(payload);

  // Feed only 10 bytes of data
  std::string partial(10, 'X');
  auto begin = partial.begin();
  auto end = partial.end();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);

  EXPECT_FALSE(complete);
  EXPECT_EQ(parser.size(), 10u);
}
