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
#include <string>
#include <vector>

using namespace nrpe;

// =============================================================================
// nrpe_exception
// =============================================================================

TEST(NrpeException, WhatReturnsMessage) {
  nrpe_exception ex("test error");
  EXPECT_STREQ(ex.what(), "test error");
}

// =============================================================================
// data constants
// =============================================================================

TEST(NrpeData, PacketTypeConstants) {
  EXPECT_EQ(data::unknownPacket, 0);
  EXPECT_EQ(data::queryPacket, 1);
  EXPECT_EQ(data::responsePacket, 2);
  EXPECT_EQ(data::moreResponsePacket, 3);
}

TEST(NrpeData, VersionConstants) {
  EXPECT_EQ(data::version2, 2);
  EXPECT_EQ(data::version3, 3);
}

// =============================================================================
// length helpers
// =============================================================================

TEST(NrpeLength, GetMinHeaderLength) { EXPECT_EQ(length::get_min_header_length(), sizeof(data::packet_header)); }

TEST(NrpeLength, GetPacketLengthV2) {
  std::size_t payload = 1024;
  std::size_t expected = sizeof(data::packet_v2) + payload;
  EXPECT_EQ(length::get_packet_length_v2(payload), expected);
}

TEST(NrpeLength, GetPacketLengthV3) {
  std::size_t payload = 100;
  std::size_t expected = sizeof(data::packet_v3) + payload - 1;
  EXPECT_EQ(length::get_packet_length_v3(payload), expected);
}

TEST(NrpeLength, GetPacketLengthV4) {
  std::size_t payload = 100;
  std::size_t expected = sizeof(data::packet_v3) + payload - 4;
  EXPECT_EQ(length::get_packet_length_v4(payload), expected);
}

TEST(NrpeLength, SetAndGetPayloadLength) {
  length::set_payload_length(2048);
  EXPECT_EQ(length::get_payload_length(), 2048u);
  // Restore default for other tests
  length::set_payload_length(1024);
}

TEST(NrpeLength, GetPayloadLengthFromPacketLength) {
  std::size_t payload = 512;
  std::size_t packet_len = length::get_packet_length_v2(payload);
  EXPECT_EQ(length::get_payload_length(packet_len), payload);
}

// =============================================================================
// packet — default constructor
// =============================================================================

TEST(NrpePacket, DefaultConstructor) {
  length::set_payload_length(1024);
  packet pkt;
  EXPECT_EQ(pkt.getType(), data::unknownPacket);
  EXPECT_EQ(pkt.getVersion(), data::version2);
  EXPECT_EQ(pkt.getResult(), 0);
  EXPECT_EQ(pkt.getPayload(), "");
  EXPECT_EQ(pkt.get_payload_length(), 1024u);
}

// =============================================================================
// packet — explicit field constructor
// =============================================================================

TEST(NrpePacket, ExplicitConstructor) {
  packet pkt(data::responsePacket, data::version2, 0, "OK", 1024);
  EXPECT_EQ(pkt.getType(), data::responsePacket);
  EXPECT_EQ(pkt.getVersion(), data::version2);
  EXPECT_EQ(pkt.getResult(), 0);
  EXPECT_EQ(pkt.getPayload(), "OK");
}

// =============================================================================
// packet — copy and assignment
// =============================================================================

TEST(NrpePacket, CopyConstructor) {
  packet pkt(data::queryPacket, data::version2, 1, "hello", 1024);
  packet copy(pkt);
  EXPECT_EQ(copy.getType(), data::queryPacket);
  EXPECT_EQ(copy.getVersion(), data::version2);
  EXPECT_EQ(copy.getResult(), 1);
  EXPECT_EQ(copy.getPayload(), "hello");
  EXPECT_EQ(copy.get_payload_length(), 1024u);
}

TEST(NrpePacket, AssignmentOperator) {
  packet pkt(data::responsePacket, data::version2, 2, "warning", 1024);
  packet other;
  other = pkt;
  EXPECT_EQ(other.getType(), data::responsePacket);
  EXPECT_EQ(other.getResult(), 2);
  EXPECT_EQ(other.getPayload(), "warning");
}

// =============================================================================
// packet — factory methods
// =============================================================================

TEST(NrpePacket, MakeRequestV2) {
  packet pkt = packet::make_request("check_cpu", 1024, 2);
  EXPECT_EQ(pkt.getType(), data::queryPacket);
  EXPECT_EQ(pkt.getVersion(), data::version2);
  EXPECT_EQ(pkt.getPayload(), "check_cpu");
}

TEST(NrpePacket, MakeRequestV4) {
  packet pkt = packet::make_request("check_mem", 0, 4);
  EXPECT_EQ(pkt.getType(), data::queryPacket);
  EXPECT_EQ(pkt.getVersion(), 4);
  EXPECT_EQ(pkt.getPayload(), "check_mem");
}

TEST(NrpePacket, MakeRequestInvalidVersionThrows) {
  EXPECT_THROW(packet::make_request("test", 1024, 3), nrpe_exception);
  EXPECT_THROW(packet::make_request("test", 1024, 1), nrpe_exception);
  EXPECT_THROW(packet::make_request("test", 1024, 5), nrpe_exception);
}

TEST(NrpePacket, CreateResponse) {
  packet pkt = packet::create_response(data::version2, 0, "OK - all good", 1024);
  EXPECT_EQ(pkt.getType(), data::responsePacket);
  EXPECT_EQ(pkt.getVersion(), data::version2);
  EXPECT_EQ(pkt.getResult(), 0);
  EXPECT_EQ(pkt.getPayload(), "OK - all good");
}

TEST(NrpePacket, CreateMoreResponse) {
  packet pkt = packet::create_more_response(1, "WARNING", 1024);
  EXPECT_EQ(pkt.getType(), data::moreResponsePacket);
  EXPECT_EQ(pkt.getVersion(), data::version2);
  EXPECT_EQ(pkt.getResult(), 1);
  EXPECT_EQ(pkt.getPayload(), "WARNING");
}

TEST(NrpePacket, UnknownResponse) {
  packet pkt = packet::unknown_response("something broke");
  EXPECT_EQ(pkt.getType(), data::responsePacket);
  EXPECT_EQ(pkt.getVersion(), data::version2);
  EXPECT_EQ(pkt.getResult(), 3);
  EXPECT_EQ(pkt.getPayload(), "something broke");
}

// =============================================================================
// packet — V2 serialization round-trip
// =============================================================================

TEST(NrpePacket, V2SerializeDeserialize) {
  packet original = packet::create_response(data::version2, 0, "OK", 1024);
  std::vector<char> buf = original.get_buffer();

  EXPECT_EQ(buf.size(), length::get_packet_length_v2(1024));

  packet restored(buf, 1024);
  EXPECT_EQ(restored.getType(), data::responsePacket);
  EXPECT_EQ(restored.getVersion(), data::version2);
  EXPECT_EQ(restored.getResult(), 0);
  EXPECT_EQ(restored.getPayload(), "OK");
  EXPECT_TRUE(restored.verifyCRC());
}

TEST(NrpePacket, V2QueryRoundTrip) {
  packet request = packet::make_request("check_disk", 1024, 2);
  std::vector<char> buf = request.get_buffer();
  packet restored(buf, 1024);
  EXPECT_EQ(restored.getType(), data::queryPacket);
  EXPECT_EQ(restored.getVersion(), data::version2);
  EXPECT_EQ(restored.getPayload(), "check_disk");
  EXPECT_TRUE(restored.verifyCRC());
}

TEST(NrpePacket, V2DifferentResultCodes) {
  for (int16_t rc : {0, 1, 2, 3}) {
    packet pkt = packet::create_response(data::version2, rc, "test", 1024);
    std::vector<char> buf = pkt.get_buffer();
    packet restored(buf, 1024);
    EXPECT_EQ(restored.getResult(), rc);
  }
}

// =============================================================================
// packet — V3/V4 serialization round-trip
// =============================================================================

TEST(NrpePacket, V4SerializeDeserialize) {
  packet original = packet::make_request("check_cpu", 0, 4);
  const std::vector<char> buf = original.get_buffer();

  // Deserialize from raw buffer
  packet restored(&buf[0], buf.size());
  EXPECT_EQ(restored.getType(), data::queryPacket);
  // Version 4 maps to version3 constant value (3) in data::
  EXPECT_TRUE(restored.getVersion() == 3 || restored.getVersion() == 4);
  EXPECT_EQ(restored.getPayload(), "check_cpu");
  EXPECT_TRUE(restored.verifyCRC());
}

// =============================================================================
// packet — to_string
// =============================================================================

TEST(NrpePacket, ToString) {
  packet pkt(data::responsePacket, data::version2, 0, "OK", 1024);
  std::string s = pkt.to_string();
  EXPECT_NE(s.find("type: 2"), std::string::npos);
  EXPECT_NE(s.find("version: 2"), std::string::npos);
  EXPECT_NE(s.find("result: 0"), std::string::npos);
  EXPECT_NE(s.find("payload: OK"), std::string::npos);
}

// =============================================================================
// packet — error cases
// =============================================================================

TEST(NrpePacket, ReadFromNullBufferThrows) {
  packet pkt(1024);
  EXPECT_THROW(pkt.readFrom(nullptr, 100), nrpe_exception);
}

TEST(NrpePacket, ReadFromTooShortThrows) {
  char buf[2] = {0, 0};
  packet pkt(1024);
  EXPECT_THROW(pkt.readFrom(buf, 2), nrpe_exception);
}

TEST(NrpePacket, V2PayloadTooLargeThrows) {
  // payload_length_ = 10 but payload is longer
  EXPECT_THROW(
      {
        packet pkt(data::responsePacket, data::version2, 0, std::string(15, 'x'), 10);
        pkt.get_buffer();
      },
      nrpe_exception);
}

TEST(NrpePacket, V2CorruptCrcThrows) {
  packet pkt = packet::create_response(data::version2, 0, "OK", 1024);
  std::vector<char> buf = pkt.get_buffer();
  // Corrupt CRC bytes (offset 4-7 in packet_v2)
  buf[4] ^= 0xFF;
  EXPECT_THROW(packet(buf, 1024), nrpe_exception);
}

TEST(NrpePacket, V2WrongLengthThrows) {
  packet pkt = packet::create_response(data::version2, 0, "OK", 1024);
  std::vector<char> buf = pkt.get_buffer();
  // Truncate buffer
  buf.resize(buf.size() - 10);
  EXPECT_THROW(packet(buf, 1024), nrpe_exception);
}

// =============================================================================
// packet — get_buffer returns valid vector
// =============================================================================

TEST(NrpePacket, GetBufferReturnsNonEmpty) {
  packet pkt = packet::create_response(data::version2, 0, "test", 1024);
  std::vector<char> buf = pkt.get_buffer();
  EXPECT_FALSE(buf.empty());
  EXPECT_EQ(buf.size(), length::get_packet_length_v2(1024));
}
