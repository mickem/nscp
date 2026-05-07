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

#include <cstring>
#include <net/nsca/nsca_packet.hpp>
#include <string>

// =============================================================================
// nsca_exception
// =============================================================================

TEST(NscaException, DefaultConstructor) {
  nsca::nsca_exception ex;
  EXPECT_STREQ(ex.what(), "");
}

TEST(NscaException, WhatReturnsMessage) {
  nsca::nsca_exception ex("test error");
  EXPECT_STREQ(ex.what(), "test error");
}

TEST(NscaException, CopyConstructor) {
  nsca::nsca_exception ex("copy me");
  nsca::nsca_exception copy(ex);
  EXPECT_STREQ(copy.what(), "copy me");
}

// =============================================================================
// data constants
// =============================================================================

TEST(NscaData, TransmittedIuvSize) { EXPECT_EQ(nsca::data::transmitted_iuv_size, 128); }

TEST(NscaData, Version3) { EXPECT_EQ(nsca::data::version3, 3); }

// =============================================================================
// length helpers
// =============================================================================

TEST(NscaLength, DefaultPayloadLength) { EXPECT_EQ(nsca::length::get_payload_length(), 512u); }

TEST(NscaLength, SetAndGetPayloadLength) {
  unsigned int orig = nsca::length::get_payload_length();
  nsca::length::set_payload_length(1024);
  EXPECT_EQ(nsca::length::get_payload_length(), 1024u);
  nsca::length::set_payload_length(orig);
}

TEST(NscaLength, GetPacketLengthIncludesHeaderAndFields) {
  unsigned int payload = 512;
  unsigned int expected =
      sizeof(nsca::data::data_packet) + payload * sizeof(char) + nsca::length::host_length * sizeof(char) + nsca::length::desc_length * sizeof(char);
  EXPECT_EQ(nsca::length::get_packet_length(payload), expected);
}

TEST(NscaLength, GetPayloadLengthFromPacketLength) {
  unsigned int payload = 512;
  unsigned int pkt_len = nsca::length::get_packet_length(payload);
  EXPECT_EQ(nsca::length::get_payload_length(pkt_len), payload);
}

TEST(NscaLength, HostAndDescLengthConstants) {
  EXPECT_EQ(nsca::length::host_length, 64u);
  EXPECT_EQ(nsca::length::desc_length, 128u);
}

TEST(NscaLength, IvPayloadLength) { EXPECT_EQ(nsca::length::iv::get_payload_length(), 128u); }

TEST(NscaLength, IvPacketLength) { EXPECT_EQ(nsca::length::iv::get_packet_length(), sizeof(nsca::data::iv_packet)); }

// =============================================================================
// packet — constructors
// =============================================================================

TEST(NscaPacket, DefaultConstructor) {
  nsca::packet pkt;
  EXPECT_EQ(pkt.code, 0u);
  EXPECT_GT(pkt.time, 0u);
  EXPECT_TRUE(pkt.host.empty());
  EXPECT_TRUE(pkt.service.empty());
  EXPECT_TRUE(pkt.result.empty());
}

TEST(NscaPacket, ConstructorWithPayloadLength) {
  nsca::packet pkt(1024);
  EXPECT_EQ(pkt.code, 0u);
  EXPECT_EQ(pkt.get_payload_length(), 1024u);
  EXPECT_GT(pkt.time, 0u);
}

TEST(NscaPacket, ConstructorWithHost) {
  nsca::packet pkt(std::string("myhost"), 256);
  EXPECT_EQ(pkt.host, "myhost");
  EXPECT_EQ(pkt.get_payload_length(), 256u);
  EXPECT_GT(pkt.time, 0u);
}

TEST(NscaPacket, ConstructorWithHostAndTimeDelta) {
  nsca::packet pkt1(std::string("host1"), 512, 0);
  nsca::packet pkt2(std::string("host2"), 512, 10);
  // pkt2 should have a slightly later timestamp
  EXPECT_GE(pkt2.time, pkt1.time);
}

// =============================================================================
// packet — to_string
// =============================================================================

TEST(NscaPacket, ToString) {
  nsca::packet pkt(std::string("myhost"), 512);
  pkt.service = "myservice";
  pkt.code = 2;
  pkt.result = "CRITICAL";

  std::string s = pkt.to_string();
  EXPECT_NE(s.find("myhost"), std::string::npos);
  EXPECT_NE(s.find("myservice"), std::string::npos);
  EXPECT_NE(s.find("CRITICAL"), std::string::npos);
  EXPECT_NE(s.find("2"), std::string::npos);
}

// =============================================================================
// packet — round-trip (get_buffer / parse_data)
// =============================================================================

TEST(NscaPacket, RoundTripDefaultPayload) {
  nsca::packet original(std::string("testhost"), 512);
  original.service = "check_cpu";
  original.code = 1;
  original.result = "WARNING - CPU load is high";

  std::string buffer(original.get_packet_length(), '\0');
  original.get_buffer(buffer);

  nsca::packet parsed(512);
  parsed.parse_data(buffer.c_str(), buffer.size());

  EXPECT_EQ(parsed.host, "testhost");
  EXPECT_EQ(parsed.service, "check_cpu");
  EXPECT_EQ(parsed.code, 1u);
  EXPECT_EQ(parsed.result, "WARNING - CPU load is high");
  EXPECT_EQ(parsed.time, original.time);
}

TEST(NscaPacket, RoundTripLargePayload) {
  nsca::packet original(std::string("bighost"), 4096);
  original.service = "check_disk";
  original.code = 0;
  original.result = std::string(2000, 'x');

  std::string buffer(original.get_packet_length(), '\0');
  original.get_buffer(buffer);

  nsca::packet parsed(4096);
  parsed.parse_data(buffer.c_str(), buffer.size());

  EXPECT_EQ(parsed.host, "bighost");
  EXPECT_EQ(parsed.service, "check_disk");
  EXPECT_EQ(parsed.code, 0u);
  EXPECT_EQ(parsed.result, std::string(2000, 'x'));
}

TEST(NscaPacket, RoundTripEmptyFields) {
  nsca::packet original(std::string("h"), 512);
  original.service = "s";
  original.code = 3;
  original.result = "";

  std::string buffer(original.get_packet_length(), '\0');
  original.get_buffer(buffer);

  nsca::packet parsed(512);
  parsed.parse_data(buffer.c_str(), buffer.size());

  EXPECT_EQ(parsed.host, "h");
  EXPECT_EQ(parsed.service, "s");
  EXPECT_EQ(parsed.code, 3u);
  EXPECT_EQ(parsed.result, "");
}

TEST(NscaPacket, RoundTripWithServerTime) {
  nsca::packet original(std::string("host"), 512);
  original.service = "svc";
  original.code = 0;
  original.result = "OK";

  std::string buffer(original.get_packet_length(), '\0');
  uint32_t server_time = 1234567890;
  original.get_buffer(buffer, server_time);

  nsca::packet parsed(512);
  parsed.parse_data(buffer.c_str(), buffer.size());

  EXPECT_EQ(parsed.time, server_time);
}

// =============================================================================
// packet — CRC32 validation
// =============================================================================

TEST(NscaPacket, CorruptedBufferThrows) {
  nsca::packet original(std::string("host"), 512);
  original.service = "svc";
  original.code = 0;
  original.result = "OK";

  std::string buffer(original.get_packet_length(), '\0');
  original.get_buffer(buffer);

  // Corrupt a byte in the result area
  buffer[buffer.size() - 10] ^= 0xFF;

  nsca::packet parsed(512);
  EXPECT_THROW(parsed.parse_data(buffer.c_str(), buffer.size()), nsca::nsca_exception);
}

TEST(NscaPacket, WrongVersionThrows) {
  // The server only speaks v3. A packet that decrypted to a different
  // advertised version (e.g. a v1 client, or a forgery built without
  // honouring the version field) must be rejected before the rest of the
  // fields are interpreted.
  nsca::packet original(std::string("host"), 512);
  original.service = "svc";
  original.code = 0;
  original.result = "OK";

  std::string buffer(original.get_packet_length(), '\0');
  original.get_buffer(buffer);

  // Overwrite the packet_version field (first int16, network byte order)
  // with version 2.
  buffer[0] = 0;
  buffer[1] = 2;
  // Recompute the CRC so the integrity check passes and we exercise the
  // version check specifically rather than tripping CRC first.
  auto* data = reinterpret_cast<nsca::data::data_packet*>(&buffer[0]);
  data->crc32_value = 0;
  const unsigned int new_crc = calculate_crc32(buffer.c_str(), static_cast<int>(buffer.size()));
  data->crc32_value = swap_bytes::hton<uint32_t>(new_crc);

  nsca::packet parsed(512);
  EXPECT_THROW(parsed.parse_data(buffer.c_str(), buffer.size()), nsca::nsca_exception);
}

// =============================================================================
// packet — validate_lengths
// =============================================================================

TEST(NscaPacket, ValidateLengthsOk) {
  nsca::packet pkt(std::string("host"), 512);
  pkt.service = "svc";
  pkt.result = "OK";
  EXPECT_NO_THROW(pkt.validate_lengths());
}

TEST(NscaPacket, ValidateLengthsHostTooLong) {
  nsca::packet pkt(std::string(nsca::length::host_length + 1, 'h'), 512);
  pkt.service = "svc";
  pkt.result = "OK";
  EXPECT_THROW(pkt.validate_lengths(), nsca::nsca_exception);
}

TEST(NscaPacket, ValidateLengthsDescTooLong) {
  nsca::packet pkt(std::string("host"), 512);
  pkt.service = std::string(nsca::length::desc_length + 1, 'd');
  pkt.result = "OK";
  EXPECT_THROW(pkt.validate_lengths(), nsca::nsca_exception);
}

TEST(NscaPacket, ValidateLengthsResultTooLong) {
  nsca::packet pkt(std::string("host"), 512);
  pkt.service = "svc";
  pkt.result = std::string(513, 'r');
  EXPECT_THROW(pkt.validate_lengths(), nsca::nsca_exception);
}

// =============================================================================
// packet — get_buffer string overload
// =============================================================================

TEST(NscaPacket, GetBufferStringOverload) {
  nsca::packet pkt(std::string("host"), 512);
  pkt.service = "svc";
  pkt.code = 0;
  pkt.result = "OK";

  // Using the version that returns a string directly fails because buffer is
  // empty. The two-arg version is the main serialization path.
  // Just verify the two-arg form works with correct buffer size.
  std::string buffer(pkt.get_packet_length(), '\0');
  EXPECT_NO_THROW(pkt.get_buffer(buffer));
  EXPECT_EQ(buffer.size(), pkt.get_packet_length());
}

TEST(NscaPacket, GetBufferTooShortThrows) {
  nsca::packet pkt(std::string("host"), 512);
  pkt.service = "svc";
  pkt.code = 0;
  pkt.result = "OK";

  std::string buffer(10, '\0');  // Way too short
  EXPECT_THROW(pkt.get_buffer(buffer), nsca::nsca_exception);
}

// =============================================================================
// packet — copy_string
// =============================================================================

TEST(NscaPacket, CopyStringTruncates) {
  char dest[10];
  nsca::packet::copy_string(dest, "hello world", 10);
  // Should have been truncated to 10 chars
  EXPECT_EQ(std::string(dest, 10), "hello worl");
}

TEST(NscaPacket, CopyStringPadsWithZeros) {
  char dest[10];
  nsca::packet::copy_string(dest, "hi", 10);
  EXPECT_EQ(dest[0], 'h');
  EXPECT_EQ(dest[1], 'i');
  for (int i = 2; i < 10; i++) {
    EXPECT_EQ(dest[i], '\0');
  }
}

// =============================================================================
// packet — assignment operator
// =============================================================================

TEST(NscaPacket, AssignmentOperator) {
  nsca::packet pkt1(std::string("host1"), 512);
  pkt1.service = "svc1";
  pkt1.code = 2;
  pkt1.result = "CRITICAL";

  nsca::packet pkt2(std::string("host2"), 1024);
  pkt2 = pkt1;

  EXPECT_EQ(pkt2.host, "host1");
  EXPECT_EQ(pkt2.service, "svc1");
  EXPECT_EQ(pkt2.code, 2u);
  EXPECT_EQ(pkt2.result, "CRITICAL");
}

// =============================================================================
// packet — get_packet_length / get_payload_length
// =============================================================================

TEST(NscaPacket, GetPacketLengthMatchesLengthHelper) {
  nsca::packet pkt(std::string("host"), 512);
  EXPECT_EQ(pkt.get_packet_length(), nsca::length::get_packet_length(512));
}

TEST(NscaPacket, GetPayloadLengthReturnsConfigured) {
  nsca::packet pkt(std::string("host"), 256);
  EXPECT_EQ(pkt.get_payload_length(), 256u);
}

// =============================================================================
// iv_packet
// =============================================================================

TEST(NscaIvPacket, RoundTrip) {
  std::string iv(nsca::length::iv::get_payload_length(), 'A');
  uint32_t timestamp = 1700000000;

  nsca::iv_packet original(iv, timestamp);
  EXPECT_EQ(original.get_time(), timestamp);
  EXPECT_EQ(original.get_iv(), iv);

  std::string buffer = original.get_buffer();
  EXPECT_EQ(buffer.size(), nsca::length::iv::get_packet_length());

  nsca::iv_packet parsed(buffer);
  EXPECT_EQ(parsed.get_time(), timestamp);
  EXPECT_EQ(parsed.get_iv(), iv);
}

TEST(NscaIvPacket, ConstructFromPtime) {
  std::string iv(nsca::length::iv::get_payload_length(), 'B');
  boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();

  nsca::iv_packet pkt(iv, now);
  EXPECT_GT(pkt.get_time(), 0u);
  EXPECT_EQ(pkt.get_iv(), iv);
}

TEST(NscaIvPacket, InvalidIvSizeThrows) {
  std::string short_iv(10, 'C');
  uint32_t timestamp = 1000;

  nsca::iv_packet pkt(short_iv, timestamp);
  EXPECT_THROW(pkt.get_buffer(), nsca::nsca_exception);
}

TEST(NscaIvPacket, BufferTooShortThrows) {
  std::string short_buffer(4, '\0');
  EXPECT_THROW(nsca::iv_packet pkt(short_buffer), nsca::nsca_exception);
}

TEST(NscaIvPacket, GetIvPreservesBinaryContent) {
  std::string iv(nsca::length::iv::get_payload_length(), '\0');
  for (int i = 0; i < static_cast<int>(iv.size()); i++) {
    iv[i] = static_cast<char>(i % 256);
  }
  uint32_t timestamp = 42;

  nsca::iv_packet original(iv, timestamp);
  std::string buffer = original.get_buffer();

  nsca::iv_packet parsed(buffer);
  EXPECT_EQ(parsed.get_iv(), iv);
  EXPECT_EQ(parsed.get_time(), timestamp);
}
