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

#include <net/nrpe/packet.hpp>
#include <net/nrpe/server/parser.hpp>
#include <string>
#include <vector>

using namespace nrpe;

// =============================================================================
// parser — basic round-trip
// =============================================================================

TEST(NrpeParser, DigestAndParseV2Packet) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  // Create a valid v2 packet
  packet original = packet::create_response(data::version2, 0, "OK - test", payload_length);
  std::vector<char> buf = original.get_buffer();

  // Feed entire buffer in one call
  char* begin = buf.data();
  char* end = begin + buf.size();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);

  EXPECT_TRUE(complete);

  packet parsed = parser.parse();
  EXPECT_EQ(parsed.getType(), data::responsePacket);
  EXPECT_EQ(parsed.getVersion(), data::version2);
  EXPECT_EQ(parsed.getResult(), 0);
  EXPECT_EQ(parsed.getPayload(), "OK - test");
}

TEST(NrpeParser, DigestAndParseV2Query) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  packet original = packet::make_request("check_cpu", payload_length, 2);
  std::vector<char> buf = original.get_buffer();

  char* begin = buf.data();
  char* end = begin + buf.size();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);

  EXPECT_TRUE(complete);

  packet parsed = parser.parse();
  EXPECT_EQ(parsed.getType(), data::queryPacket);
  EXPECT_EQ(parsed.getVersion(), data::version2);
  EXPECT_EQ(parsed.getPayload(), "check_cpu");
}

// =============================================================================
// parser — incremental feeding
// =============================================================================

TEST(NrpeParser, DigestIncrementally) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  packet original = packet::create_response(data::version2, 0, "chunked", payload_length);
  std::vector<char> buf = original.get_buffer();

  // Feed one byte at a time until complete
  bool complete = false;
  std::size_t offset = 0;
  while (offset < buf.size() && !complete) {
    char* begin = buf.data() + offset;
    char* end = begin + 1;
    boost::tie(complete, begin) = parser.digest(begin, end);
    offset++;
  }

  EXPECT_TRUE(complete);

  packet parsed = parser.parse();
  EXPECT_EQ(parsed.getPayload(), "chunked");
}

TEST(NrpeParser, DigestPartialThenRest) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  packet original = packet::create_response(data::version2, 0, "split", payload_length);
  std::vector<char> buf = original.get_buffer();

  // Feed first half
  std::size_t half = buf.size() / 2;
  char* begin = buf.data();
  char* end = begin + half;
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);
  EXPECT_FALSE(complete);

  // Feed second half
  begin = buf.data() + half;
  end = buf.data() + buf.size();
  boost::tie(complete, begin) = parser.digest(begin, end);
  EXPECT_TRUE(complete);

  packet parsed = parser.parse();
  EXPECT_EQ(parsed.getPayload(), "split");
}

// =============================================================================
// parser — reset
// =============================================================================

TEST(NrpeParser, ResetClearsState) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  // Feed partial data
  packet original = packet::create_response(data::version2, 0, "first", payload_length);
  std::vector<char> buf = original.get_buffer();
  std::size_t partial = buf.size() / 3;
  char* begin = buf.data();
  char* end = begin + partial;
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);
  EXPECT_FALSE(complete);

  // Reset and feed a new complete packet
  parser.reset();

  packet second = packet::create_response(data::version2, 1, "second", payload_length);
  std::vector<char> buf2 = second.get_buffer();
  begin = buf2.data();
  end = begin + buf2.size();
  boost::tie(complete, begin) = parser.digest(begin, end);
  EXPECT_TRUE(complete);

  packet parsed = parser.parse();
  EXPECT_EQ(parsed.getPayload(), "second");
  EXPECT_EQ(parsed.getResult(), 1);
}

// =============================================================================
// parser — read_version before data
// =============================================================================

TEST(NrpeParser, ReadVersionBeforeDataReturnsNegative) {
  server::parser parser(1024);
  EXPECT_EQ(parser.read_version(), -1);
}

TEST(NrpeParser, ReadVersionAfterPartialHeader) {
  server::parser parser(1024);

  // Feed enough for the header (version field)
  packet pkt = packet::create_response(data::version2, 0, "test", 1024);
  std::vector<char> buf = pkt.get_buffer();

  // Feed just the header bytes
  std::size_t header_size = length::get_min_header_length();
  char* begin = buf.data();
  char* end = begin + header_size;
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);

  EXPECT_EQ(parser.read_version(), data::version2);
}

// =============================================================================
// parser — V4 packet round-trip
// =============================================================================

TEST(NrpeParser, DigestAndParseV4Packet) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  packet original = packet::make_request("check_disk", 0, 4);
  std::vector<char> buf = original.get_buffer();

  char* begin = buf.data();
  char* end = begin + buf.size();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);

  EXPECT_TRUE(complete);

  packet parsed = parser.parse();
  EXPECT_EQ(parsed.getType(), data::queryPacket);
  EXPECT_EQ(parsed.getPayload(), "check_disk");
  EXPECT_TRUE(parsed.verifyCRC());
}

TEST(NrpeParser, DISABLED_DigestV4Incrementally) {
  // TODO: Fix this test
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  packet original = packet::make_request("check_mem", 0, 4);
  std::vector<char> buf = original.get_buffer();

  // Feed one byte at a time
  bool complete = false;
  std::size_t offset = 0;
  while (offset < buf.size() && !complete) {
    char* begin = buf.data() + offset;
    char* end = begin + 1;
    boost::tie(complete, begin) = parser.digest(begin, end);
    offset++;
  }

  EXPECT_TRUE(complete);

  packet parsed = parser.parse();
  EXPECT_EQ(parsed.getPayload(), "check_mem");
  EXPECT_TRUE(parsed.verifyCRC());
}

// =============================================================================
// parser — read_len
// =============================================================================

TEST(NrpeParser, ReadLenBeforeDataReturnsNegative) {
  server::parser parser(1024);
  EXPECT_EQ(parser.read_len(), -1);
}

// =============================================================================
// parser — get_packet_length accessors
// =============================================================================

TEST(NrpeParser, GetPacketLengthV2MatchesLengthHelper) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);
  EXPECT_EQ(parser.get_packet_length_v2(), length::get_packet_length_v2(payload_length));
}

TEST(NrpeParser, GetPacketLengthV2DifferentSizes) {
  server::parser parser_small(64);
  server::parser parser_large(4096);
  EXPECT_EQ(parser_small.get_packet_length_v2(), length::get_packet_length_v2(64));
  EXPECT_EQ(parser_large.get_packet_length_v2(), length::get_packet_length_v2(4096));
}

// =============================================================================
// parser — sequential packets
// =============================================================================

TEST(NrpeParser, ParseMultipleSequentialPackets) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  // First packet
  packet first_pkt = packet::create_response(data::version2, 0, "first", payload_length);
  std::vector<char> buf1 = first_pkt.get_buffer();
  char* begin = buf1.data();
  char* end = begin + buf1.size();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);
  EXPECT_TRUE(complete);
  packet parsed1 = parser.parse();
  EXPECT_EQ(parsed1.getPayload(), "first");

  // Second packet (parse() clears the buffer)
  packet second_pkt = packet::create_response(data::version2, 1, "second", payload_length);
  std::vector<char> buf2 = second_pkt.get_buffer();
  begin = buf2.data();
  end = begin + buf2.size();
  boost::tie(complete, begin) = parser.digest(begin, end);
  EXPECT_TRUE(complete);
  packet parsed2 = parser.parse();
  EXPECT_EQ(parsed2.getPayload(), "second");
  EXPECT_EQ(parsed2.getResult(), 1);
}

// =============================================================================
// parser — empty payload
// =============================================================================

TEST(NrpeParser, V2EmptyPayload) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  packet original = packet::create_response(data::version2, 0, "", payload_length);
  std::vector<char> buf = original.get_buffer();

  char* begin = buf.data();
  char* end = begin + buf.size();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);

  EXPECT_TRUE(complete);

  packet parsed = parser.parse();
  EXPECT_EQ(parsed.getPayload(), "");
  EXPECT_TRUE(parsed.verifyCRC());
}

TEST(NrpeParser, DISABLED_V4EmptyPayload) {
  // TODO: Currently empty packages are not supported
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  packet original = packet::make_request("", 0, 4);
  std::vector<char> buf = original.get_buffer();

  char* begin = buf.data();
  char* end = begin + buf.size();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);

  EXPECT_TRUE(complete);

  packet parsed = parser.parse();
  EXPECT_EQ(parsed.getPayload(), "");
  EXPECT_TRUE(parsed.verifyCRC());
}

// =============================================================================
// parser — V2 payload with null bytes (zero-padded buffer)
// =============================================================================

TEST(NrpeParser, V2PayloadStopsAtNullByte) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  packet original = packet::create_response(data::version2, 0, "hello", payload_length);
  std::vector<char> buf = original.get_buffer();

  char* begin = buf.data();
  char* end = begin + buf.size();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);
  EXPECT_TRUE(complete);

  packet parsed = parser.parse();
  // The v2 buffer is zero-padded; payload should be "hello" not "hello\0\0\0..."
  EXPECT_EQ(parsed.getPayload(), "hello");
  EXPECT_EQ(parsed.getPayload().size(), 5u);
}

// =============================================================================
// parser — read_version with V4 data
// =============================================================================

TEST(NrpeParser, ReadVersionAfterV4Header) {
  server::parser parser(1024);

  packet pkt = packet::make_request("test", 0, 4);
  std::vector<char> buf = pkt.get_buffer();

  // Feed just the header bytes
  std::size_t header_size = length::get_min_header_length();
  char* begin = buf.data();
  char* end = begin + header_size;
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);

  // Version 4 uses the same wire constant as version3 (3)
  // TODO: Here we should ideally use the constants
  EXPECT_EQ(parser.read_version(), 4);
}

// =============================================================================
// parser — digest consumes all input
// =============================================================================

TEST(NrpeParser, DigestConsumesExactBuffer) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  packet original = packet::create_response(data::version2, 0, "exact", payload_length);
  std::vector<char> buf = original.get_buffer();

  char* begin = buf.data();
  char* end = begin + buf.size();
  char* returned_it;
  bool complete;
  boost::tie(complete, returned_it) = parser.digest(begin, end);

  EXPECT_TRUE(complete);
  // Iterator should have advanced to end (all bytes consumed)
  EXPECT_EQ(returned_it, end);
}

// =============================================================================
// parser — DoS guard (H7)
//
// A v3/v4 header with an oversized buffer_length must not cause the parser
// to grow its internal buffer beyond the documented 1 MiB cap. The decoder
// in packet.cpp already rejects payloads above that ceiling; the parser
// must not pin nearly 2 MiB of memory in the meantime.
// =============================================================================

TEST(NrpeParser, DigestRejectsOversizedV3Header) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  // Hand-craft a v3 header that advertises a buffer_length larger than the
  // 1 MiB cap. Use raw bytes so we exercise the size_t conversion path.
  // header layout starts with: int16 version (network byte order) followed
  // by other fields up to int32 buffer_length at the documented offset.
  std::vector<char> hdr(sizeof(data::packet_v3), 0);
  data::packet_v3* p = reinterpret_cast<data::packet_v3*>(hdr.data());
  p->packet_version = swap_bytes::hton<int16_t>(data::version3);
  p->buffer_length = swap_bytes::hton<int32_t>(static_cast<int32_t>(2 * 1024 * 1024));  // 2 MiB

  char* begin = hdr.data();
  char* end = begin + hdr.size();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);

  // The parser must short-circuit "complete" so the protocol layer drops
  // the connection. parse() afterwards is expected to throw, but the key
  // assertion here is that the buffer never grew past the cap.
  EXPECT_TRUE(complete);
  EXPECT_LE(parser.size(), 1024u * 1024u);
}

TEST(NrpeParser, DigestRejectsNegativeBufferLength) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  std::vector<char> hdr(sizeof(data::packet_v3), 0);
  data::packet_v3* p = reinterpret_cast<data::packet_v3*>(hdr.data());
  p->packet_version = swap_bytes::hton<int16_t>(data::version3);
  p->buffer_length = swap_bytes::hton<int32_t>(static_cast<int32_t>(-1));

  char* begin = hdr.data();
  char* end = begin + hdr.size();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);

  // Negative advertised length is treated as a hard error - the parser
  // must not implicitly cast it to a huge size_t and try to read that many
  // bytes.
  EXPECT_TRUE(complete);
  EXPECT_LE(parser.size(), 1024u * 1024u);
}
