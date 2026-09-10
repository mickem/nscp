// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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

TEST(NrpeParser, DigestV4Incrementally) {
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

TEST(NrpeParser, V4EmptyPayload) {
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
// parser — digest must not claim completeness on a half-filled buffer
//
// digest() used to short-circuit with "complete" whenever the computed
// packet length was below 1024 bytes. That is a length sanity check written
// as a completeness answer, and it fires in two entirely legitimate cases:
// a v3/v4 packet smaller than 1024 bytes that arrives split across TCP
// reads, and any deployment whose `payload length` is below 1012 (which
// makes *every* v2 packet shorter than 1024). In both cases the decoder is
// handed a half-read buffer and fails its length/CRC check.
// =============================================================================

TEST(NrpeParser, DigestV2IsNotCompleteUntilFullWithSmallPayloadLength) {
  // 12 + 64 = 76 bytes on the wire, i.e. well under the old 1024 cut-off.
  const unsigned int payload_length = 64;
  server::parser parser(payload_length);

  packet original = packet::create_response(data::version2, 0, "small", payload_length);
  std::vector<char> buf = original.get_buffer();
  ASSERT_LT(buf.size(), 1024u);

  const std::size_t first_chunk = buf.size() / 2;
  char* begin = buf.data();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, begin + first_chunk);
  EXPECT_FALSE(complete) << "digest() reported a complete packet after " << parser.size() << " of " << buf.size() << " bytes";

  begin = buf.data() + first_chunk;
  boost::tie(complete, begin) = parser.digest(begin, buf.data() + buf.size());
  EXPECT_TRUE(complete);

  packet parsed = parser.parse();
  EXPECT_EQ(parsed.getPayload(), "small");
  EXPECT_TRUE(parsed.verifyCRC());
}

TEST(NrpeParser, DigestV3SplitAcrossReadsIsNotCompleteUntilFull) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  packet original = packet::create_response(data::version3, 0, "abc", payload_length);
  std::vector<char> buf = original.get_buffer();
  ASSERT_LT(buf.size(), 1024u);

  // Split inside the fixed header so the advertised length is not yet known.
  const std::size_t first_chunk = 10;
  char* begin = buf.data();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, begin + first_chunk);
  EXPECT_FALSE(complete) << "digest() reported a complete packet after " << parser.size() << " of " << buf.size() << " bytes";

  begin = buf.data() + first_chunk;
  boost::tie(complete, begin) = parser.digest(begin, buf.data() + buf.size());
  EXPECT_TRUE(complete);

  packet parsed = parser.parse();
  EXPECT_EQ(parsed.getPayload(), "abc");
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

  EXPECT_EQ(parser.read_version(), data::version4);
}

// =============================================================================
// parser — a v4 packet larger than the configured v2 packet length
//
// NRPE v3 and v4 carry the same fields; they differ only in whether the
// three alignment bytes at the end of the struct are part of the wire
// packet. The *version field* however really does carry 4, so the parser
// has to recognise it. `data::version4` used to be defined as 3, which made
// the v3/v4 arm of digest() read `v == 3 || v == 3`: a packet announcing
// version 4 matched neither arm and no bytes were consumed.
//
// Small v4 packets survived that by accident (the first digest() call runs
// through the `v == -1` arm and buffers a whole v2 packet worth of bytes).
// A v4 packet longer than the v2 packet length does not: the first call
// stops at get_packet_length_v2() bytes, the next consumes nothing, and
// read_protocol::on_read sees an unmoved iterator and drops the connection
// with "Digester failed to parse NRPE data ... giving up".
// =============================================================================

TEST(NrpeParser, DigestV4PacketLongerThanV2PacketLength) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  // Deliberately longer than length::get_packet_length_v2(1024) == 1036.
  const std::string payload(2000, 'x');
  packet original = packet::create_response(4, 0, payload, payload_length);
  std::vector<char> buf = original.get_buffer();
  ASSERT_GT(buf.size(), length::get_packet_length_v2(payload_length));

  // Mimic read_protocol::on_read: keep digesting while the parser consumes.
  char* begin = buf.data();
  char* end = begin + buf.size();
  bool complete = false;
  while (begin != end && !complete) {
    char* const old_begin = begin;
    boost::tie(complete, begin) = parser.digest(begin, end);
    // An unmoved iterator is what makes on_read give up on the connection.
    ASSERT_TRUE(complete || begin != old_begin) << "digest() stalled after " << parser.size() << " bytes";
  }

  ASSERT_TRUE(complete);
  packet parsed = parser.parse();
  EXPECT_EQ(parsed.getVersion(), 4);
  EXPECT_EQ(parsed.getPayload(), payload);
  EXPECT_TRUE(parsed.verifyCRC());
}

// =============================================================================
// parser — a short v4 packet followed by unprotected trailing bytes
//
// digest() fills a whole v2 packet worth of bytes through the `v == -1` arm
// before the version is known, so a small v4 packet reaches parse() with
// whatever else was in that read still behind it. Only the declared payload
// is covered by the packet's CRC, so the trailing bytes must not end up in
// the command.
// =============================================================================

TEST(NrpeParser, V4TrailingBytesDoNotBecomeThePayload) {
  const unsigned int payload_length = 1024;
  server::parser parser(payload_length);

  packet original = packet::create_response(4, 0, "ok", payload_length);
  std::vector<char> buf = original.get_buffer();
  ASSERT_LT(buf.size(), length::get_packet_length_v2(payload_length));
  buf.resize(length::get_packet_length_v2(payload_length), 'X');

  char* begin = buf.data();
  char* end = begin + buf.size();
  bool complete;
  boost::tie(complete, begin) = parser.digest(begin, end);
  ASSERT_TRUE(complete);

  packet parsed = parser.parse();
  EXPECT_EQ(parsed.getPayload(), "ok");
  EXPECT_TRUE(parsed.verifyCRC());
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
  p->packet_version = boost::endian::native_to_big(static_cast<int16_t>(data::version3));
  p->buffer_length = boost::endian::native_to_big(static_cast<int32_t>(2 * 1024 * 1024));  // 2 MiB

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
  p->packet_version = boost::endian::native_to_big(static_cast<int16_t>(data::version3));
  p->buffer_length = boost::endian::native_to_big(static_cast<int32_t>(-1));

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
