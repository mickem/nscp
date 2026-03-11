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

#include <net/icmp_header.hpp>
#include <net/ipv4_header.hpp>
#include <net/pinger.hpp>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// result_container tests
// ============================================================================

TEST(ResultContainer, DefaultConstruction) {
  const result_container r;
  EXPECT_EQ(0u, r.num_send_);
  EXPECT_EQ(0u, r.num_replies_);
  EXPECT_EQ(0u, r.num_timeouts_);
  EXPECT_EQ(0u, r.length_);
  EXPECT_EQ(0, r.sequence_number_);
  EXPECT_EQ(0, r.ttl_);
  EXPECT_EQ(0u, r.time_);
  EXPECT_TRUE(r.destination_.empty());
  EXPECT_TRUE(r.ip_.empty());
}

TEST(ResultContainer, MembersAreAssignable) {
  result_container r;
  r.destination_ = "example.com";
  r.ip_ = "93.184.216.34";
  r.num_send_ = 5;
  r.num_replies_ = 4;
  r.num_timeouts_ = 1;
  r.length_ = 64;
  r.sequence_number_ = 3;
  r.ttl_ = 128;
  r.time_ = 42;

  EXPECT_EQ("example.com", r.destination_);
  EXPECT_EQ("93.184.216.34", r.ip_);
  EXPECT_EQ(5u, r.num_send_);
  EXPECT_EQ(4u, r.num_replies_);
  EXPECT_EQ(1u, r.num_timeouts_);
  EXPECT_EQ(64u, r.length_);
  EXPECT_EQ(3, r.sequence_number_);
  EXPECT_EQ(128, r.ttl_);
  EXPECT_EQ(42u, r.time_);
}

// ============================================================================
// icmp_header tests
// ============================================================================

TEST(IcmpHeader, DefaultConstruction) {
  const icmp_header hdr;
  EXPECT_EQ(0, hdr.type());
  EXPECT_EQ(0, hdr.code());
  EXPECT_EQ(0, hdr.checksum());
  EXPECT_EQ(0, hdr.identifier());
  EXPECT_EQ(0, hdr.sequence_number());
}

TEST(IcmpHeader, SetType) {
  icmp_header hdr;
  hdr.type(icmp_header::echo_request);
  EXPECT_EQ(icmp_header::echo_request, hdr.type());
  EXPECT_EQ(8, hdr.type());
}

TEST(IcmpHeader, SetCode) {
  icmp_header hdr;
  hdr.code(3);
  EXPECT_EQ(3, hdr.code());
}

TEST(IcmpHeader, SetChecksum) {
  icmp_header hdr;
  hdr.checksum(0xABCD);
  EXPECT_EQ(0xABCD, hdr.checksum());
}

TEST(IcmpHeader, SetIdentifier) {
  icmp_header hdr;
  hdr.identifier(12345);
  EXPECT_EQ(12345, hdr.identifier());
}

TEST(IcmpHeader, SetSequenceNumber) {
  icmp_header hdr;
  hdr.sequence_number(42);
  EXPECT_EQ(42, hdr.sequence_number());
}

TEST(IcmpHeader, SetIdentifierMaxValue) {
  icmp_header hdr;
  hdr.identifier(0xFFFF);
  EXPECT_EQ(0xFFFF, hdr.identifier());
}

TEST(IcmpHeader, SetSequenceNumberMaxValue) {
  icmp_header hdr;
  hdr.sequence_number(0xFFFF);
  EXPECT_EQ(0xFFFF, hdr.sequence_number());
}

TEST(IcmpHeader, MultipleFieldsAreIndependent) {
  icmp_header hdr;
  hdr.type(icmp_header::echo_reply);
  hdr.code(5);
  hdr.checksum(0x1234);
  hdr.identifier(0x5678);
  hdr.sequence_number(0x9ABC);

  EXPECT_EQ(icmp_header::echo_reply, hdr.type());
  EXPECT_EQ(5, hdr.code());
  EXPECT_EQ(0x1234, hdr.checksum());
  EXPECT_EQ(0x5678, hdr.identifier());
  EXPECT_EQ(0x9ABC, hdr.sequence_number());
}

TEST(IcmpHeader, TypeConstants) {
  EXPECT_EQ(0, icmp_header::echo_reply);
  EXPECT_EQ(3, icmp_header::destination_unreachable);
  EXPECT_EQ(4, icmp_header::source_quench);
  EXPECT_EQ(5, icmp_header::redirect);
  EXPECT_EQ(8, icmp_header::echo_request);
  EXPECT_EQ(11, icmp_header::time_exceeded);
  EXPECT_EQ(12, icmp_header::parameter_problem);
  EXPECT_EQ(13, icmp_header::timestamp_request);
  EXPECT_EQ(14, icmp_header::timestamp_reply);
  EXPECT_EQ(15, icmp_header::info_request);
  EXPECT_EQ(16, icmp_header::info_reply);
  EXPECT_EQ(17, icmp_header::address_request);
  EXPECT_EQ(18, icmp_header::address_reply);
}

// ============================================================================
// icmp_header serialization round-trip
// ============================================================================

TEST(IcmpHeader, SerializeDeserializeRoundTrip) {
  icmp_header original;
  original.type(icmp_header::echo_request);
  original.code(0);
  original.identifier(0x1234);
  original.sequence_number(0x0001);
  original.checksum(0xABCD);

  // Serialize to stream
  std::stringstream ss;
  ss << original;
  EXPECT_EQ(8u, ss.str().size());

  // Deserialize from stream
  icmp_header parsed;
  ss >> parsed;
  EXPECT_TRUE(ss.good());

  EXPECT_EQ(original.type(), parsed.type());
  EXPECT_EQ(original.code(), parsed.code());
  EXPECT_EQ(original.checksum(), parsed.checksum());
  EXPECT_EQ(original.identifier(), parsed.identifier());
  EXPECT_EQ(original.sequence_number(), parsed.sequence_number());
}

TEST(IcmpHeader, DeserializeFromTooShortStream) {
  std::stringstream ss;
  ss.write("\x08\x00\x00", 3);  // only 3 bytes, need 8

  icmp_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.fail());
}

// ============================================================================
// compute_checksum tests
// ============================================================================

TEST(IcmpHeader, ComputeChecksumEmptyBody) {
  icmp_header hdr;
  hdr.type(icmp_header::echo_request);
  hdr.code(0);
  hdr.identifier(0x0001);
  hdr.sequence_number(0x0001);

  std::string body;
  compute_checksum(hdr, body.begin(), body.end());

  // Checksum should be non-zero for a valid header
  EXPECT_NE(0, hdr.checksum());
}

TEST(IcmpHeader, ComputeChecksumIsConsistent) {
  icmp_header hdr1;
  hdr1.type(icmp_header::echo_request);
  hdr1.code(0);
  hdr1.identifier(0x1234);
  hdr1.sequence_number(0x0005);

  std::string body = "Hello, World!";
  compute_checksum(hdr1, body.begin(), body.end());

  icmp_header hdr2;
  hdr2.type(icmp_header::echo_request);
  hdr2.code(0);
  hdr2.identifier(0x1234);
  hdr2.sequence_number(0x0005);
  compute_checksum(hdr2, body.begin(), body.end());

  EXPECT_EQ(hdr1.checksum(), hdr2.checksum());
}

TEST(IcmpHeader, ComputeChecksumDifferentBodiesProduceDifferentChecksums) {
  auto make_header = []() {
    icmp_header hdr;
    hdr.type(icmp_header::echo_request);
    hdr.code(0);
    hdr.identifier(0x0001);
    hdr.sequence_number(0x0001);
    return hdr;
  };

  icmp_header hdr1 = make_header();
  std::string body1 = "AAAA";
  compute_checksum(hdr1, body1.begin(), body1.end());

  icmp_header hdr2 = make_header();
  std::string body2 = "BBBB";
  compute_checksum(hdr2, body2.begin(), body2.end());

  EXPECT_NE(hdr1.checksum(), hdr2.checksum());
}

TEST(IcmpHeader, ComputeChecksumDifferentSequenceNumbersProduceDifferentChecksums) {
  std::string body = "test";

  icmp_header hdr1;
  hdr1.type(icmp_header::echo_request);
  hdr1.code(0);
  hdr1.identifier(0x0001);
  hdr1.sequence_number(1);
  compute_checksum(hdr1, body.begin(), body.end());

  icmp_header hdr2;
  hdr2.type(icmp_header::echo_request);
  hdr2.code(0);
  hdr2.identifier(0x0001);
  hdr2.sequence_number(2);
  compute_checksum(hdr2, body.begin(), body.end());

  EXPECT_NE(hdr1.checksum(), hdr2.checksum());
}

TEST(IcmpHeader, ComputeChecksumOddLengthBody) {
  icmp_header hdr;
  hdr.type(icmp_header::echo_request);
  hdr.code(0);
  hdr.identifier(0x0001);
  hdr.sequence_number(0x0001);

  std::string body = "ABC";  // odd length
  compute_checksum(hdr, body.begin(), body.end());
  EXPECT_NE(0, hdr.checksum());
}

TEST(IcmpHeader, ComputeChecksumVerifyOnesComplement) {
  // After computing checksum, summing the entire header+body should yield 0xFFFF
  // (one's complement arithmetic)
  icmp_header hdr;
  hdr.type(icmp_header::echo_request);
  hdr.code(0);
  hdr.identifier(0x0001);
  hdr.sequence_number(0x0001);

  std::string body = "test data";
  compute_checksum(hdr, body.begin(), body.end());

  // Verify: sum type+code, checksum, identifier, sequence_number, and body
  unsigned int sum = (hdr.type() << 8) + hdr.code() + hdr.checksum() + hdr.identifier() + hdr.sequence_number();

  auto it = body.begin();
  while (it != body.end()) {
    sum += (static_cast<unsigned char>(*it++) << 8);
    if (it != body.end()) sum += static_cast<unsigned char>(*it++);
  }
  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);

  EXPECT_EQ(0xFFFF, static_cast<unsigned short>(sum));
}

// ============================================================================
// ipv4_header tests
// ============================================================================

TEST(Ipv4Header, DefaultConstruction) {
  const ipv4_header hdr;
  EXPECT_EQ(0, hdr.version());
  EXPECT_EQ(0, hdr.header_length());
  EXPECT_EQ(0, hdr.type_of_service());
  EXPECT_EQ(0, hdr.total_length());
  EXPECT_EQ(0, hdr.identification());
  EXPECT_FALSE(hdr.dont_fragment());
  EXPECT_FALSE(hdr.more_fragments());
  EXPECT_EQ(0, hdr.fragment_offset());
  EXPECT_EQ(0, hdr.time_to_live());
  EXPECT_EQ(0, hdr.protocol());
  EXPECT_EQ(0, hdr.header_checksum());
}

// Build a minimal valid IPv4 header byte array (20 bytes, no options)
static std::string make_ipv4_header_bytes(unsigned char ttl, unsigned char protocol, const std::string &src_ip_str, const std::string &dst_ip_str,
                                          unsigned short total_len = 20, unsigned short identification = 0, bool dont_fragment = false,
                                          bool more_fragments = false, unsigned short fragment_offset = 0) {
  unsigned char bytes[20] = {};
  // Version (4) + IHL (5 = 20 bytes)
  bytes[0] = 0x45;
  // TOS
  bytes[1] = 0;
  // Total length
  bytes[2] = static_cast<unsigned char>(total_len >> 8);
  bytes[3] = static_cast<unsigned char>(total_len & 0xFF);
  // Identification
  bytes[4] = static_cast<unsigned char>(identification >> 8);
  bytes[5] = static_cast<unsigned char>(identification & 0xFF);
  // Flags + fragment offset
  unsigned short flags_frag = fragment_offset & 0x1FFF;
  if (dont_fragment) flags_frag |= 0x4000;
  if (more_fragments) flags_frag |= 0x2000;
  bytes[6] = static_cast<unsigned char>(flags_frag >> 8);
  bytes[7] = static_cast<unsigned char>(flags_frag & 0xFF);
  // TTL
  bytes[8] = ttl;
  // Protocol
  bytes[9] = protocol;
  // Header checksum (leave as 0 for test purposes)
  bytes[10] = 0;
  bytes[11] = 0;

  // Parse IP addresses
  const auto src = boost::asio::ip::address_v4::from_string(src_ip_str).to_bytes();
  const auto dst = boost::asio::ip::address_v4::from_string(dst_ip_str).to_bytes();
  for (int i = 0; i < 4; ++i) {
    bytes[12 + i] = src[i];
    bytes[16 + i] = dst[i];
  }

  return {reinterpret_cast<const char *>(bytes), 20};
}

TEST(Ipv4Header, ParseMinimalHeader) {
  const std::string raw = make_ipv4_header_bytes(64, 1, "192.168.1.1", "10.0.0.1");

  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;

  EXPECT_TRUE(ss.good());
  EXPECT_EQ(4, hdr.version());
  EXPECT_EQ(20, hdr.header_length());
  EXPECT_EQ(64, hdr.time_to_live());
  EXPECT_EQ(1, hdr.protocol());  // ICMP
  EXPECT_EQ(boost::asio::ip::address_v4::from_string("192.168.1.1"), hdr.source_address());
  EXPECT_EQ(boost::asio::ip::address_v4::from_string("10.0.0.1"), hdr.destination_address());
}

TEST(Ipv4Header, ParseTTLValues) {
  const unsigned char ttl_values[] = {0, 1, 64, 128, 255};
  for (unsigned char ttl : ttl_values) {
    std::string raw = make_ipv4_header_bytes(ttl, 6, "127.0.0.1", "127.0.0.1");
    std::stringstream ss(raw);
    ipv4_header hdr;
    ss >> hdr;
    EXPECT_TRUE(ss.good());
    EXPECT_EQ(ttl, hdr.time_to_live());
  }
}

TEST(Ipv4Header, ParseProtocol) {
  // ICMP = 1, TCP = 6, UDP = 17
  constexpr unsigned char protos[] = {1, 6, 17};
  for (unsigned char proto : protos) {
    std::string raw = make_ipv4_header_bytes(128, proto, "10.0.0.1", "10.0.0.2");
    std::stringstream ss(raw);
    ipv4_header hdr;
    ss >> hdr;
    EXPECT_TRUE(ss.good());
    EXPECT_EQ(proto, hdr.protocol());
  }
}

TEST(Ipv4Header, ParseTotalLength) {
  const std::string raw = make_ipv4_header_bytes(64, 1, "1.2.3.4", "5.6.7.8", 1500);
  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.good());
  EXPECT_EQ(1500, hdr.total_length());
}

TEST(Ipv4Header, ParseIdentification) {
  const std::string raw = make_ipv4_header_bytes(64, 1, "1.2.3.4", "5.6.7.8", 20, 0xABCD);
  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.good());
  EXPECT_EQ(0xABCD, hdr.identification());
}

TEST(Ipv4Header, ParseDontFragmentFlag) {
  const std::string raw = make_ipv4_header_bytes(64, 1, "1.2.3.4", "5.6.7.8", 20, 0, true, false);
  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.good());
  EXPECT_TRUE(hdr.dont_fragment());
  EXPECT_FALSE(hdr.more_fragments());
}

TEST(Ipv4Header, ParseMoreFragmentsFlag) {
  const std::string raw = make_ipv4_header_bytes(64, 1, "1.2.3.4", "5.6.7.8", 20, 0, false, true);
  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.good());
  EXPECT_FALSE(hdr.dont_fragment());
  EXPECT_TRUE(hdr.more_fragments());
}

TEST(Ipv4Header, ParseBothFlags) {
  const std::string raw = make_ipv4_header_bytes(64, 1, "1.2.3.4", "5.6.7.8", 20, 0, true, true);
  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.good());
  EXPECT_TRUE(hdr.dont_fragment());
  EXPECT_TRUE(hdr.more_fragments());
}

TEST(Ipv4Header, ParseFragmentOffset) {
  const std::string raw = make_ipv4_header_bytes(64, 1, "1.2.3.4", "5.6.7.8", 20, 0, false, false, 185);
  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.good());
  EXPECT_EQ(185, hdr.fragment_offset());
}

TEST(Ipv4Header, ParseSourceAddress) {
  const std::string raw = make_ipv4_header_bytes(64, 1, "172.16.254.1", "8.8.8.8");
  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.good());
  EXPECT_EQ(boost::asio::ip::address_v4::from_string("172.16.254.1"), hdr.source_address());
}

TEST(Ipv4Header, ParseDestinationAddress) {
  const std::string raw = make_ipv4_header_bytes(64, 1, "10.0.0.1", "192.168.100.200");
  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.good());
  EXPECT_EQ(boost::asio::ip::address_v4::from_string("192.168.100.200"), hdr.destination_address());
}

TEST(Ipv4Header, ParseWrongVersionFailsStream) {
  // Build a header with version 6 instead of 4
  std::string raw = make_ipv4_header_bytes(64, 1, "1.2.3.4", "5.6.7.8");
  raw[0] = 0x65;  // version 6, IHL 5
  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.fail());
}

TEST(Ipv4Header, ParseTooShortStreamFails) {
  std::stringstream ss;
  ss.write("\x45\x00\x00\x14\x00\x00", 6);  // only 6 bytes, need 20
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.fail());
}

TEST(Ipv4Header, ParseWithOptions) {
  // Build a header with IHL = 6 (24 bytes = 20 + 4 bytes options)
  std::string raw = make_ipv4_header_bytes(64, 1, "1.2.3.4", "5.6.7.8");
  raw[0] = 0x46;  // version 4, IHL 6 (24 bytes)
  // Append 4 bytes of options
  raw.append(4, '\0');
  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.good());
  EXPECT_EQ(4, hdr.version());
  EXPECT_EQ(24, hdr.header_length());
}

TEST(Ipv4Header, ParseInvalidIhlTooLargeFails) {
  // IHL = 0xF = 15, meaning 60 bytes header, but options_length = 40
  std::string raw = make_ipv4_header_bytes(64, 1, "1.2.3.4", "5.6.7.8");
  raw[0] = 0x4F;  // version 4, IHL 15 (60 bytes)
  // Only 20 bytes provided, need 60 → stream should fail due to insufficient data
  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.fail());
}

// ============================================================================
// Combined ipv4 + icmp header parsing (simulating a received ICMP echo reply)
// ============================================================================

TEST(IcmpEchoReply, ParseCombinedIpv4AndIcmpHeaders) {
  // Build IPv4 header
  std::string ip_raw = make_ipv4_header_bytes(64, 1, "8.8.8.8", "192.168.1.100", 28);

  // Build ICMP echo reply header
  icmp_header icmp_reply;
  icmp_reply.type(icmp_header::echo_reply);
  icmp_reply.code(0);
  icmp_reply.identifier(0x1234);
  icmp_reply.sequence_number(0x0001);
  icmp_reply.checksum(0);  // not verifying checksum in this test

  std::stringstream icmp_ss;
  icmp_ss << icmp_reply;
  std::string icmp_raw = icmp_ss.str();

  // Combine
  std::string combined = ip_raw + icmp_raw;
  std::stringstream ss(combined);

  ipv4_header ipv4_hdr;
  icmp_header icmp_hdr;
  ss >> ipv4_hdr >> icmp_hdr;

  EXPECT_TRUE(ss.good());
  EXPECT_EQ(4, ipv4_hdr.version());
  EXPECT_EQ(64, ipv4_hdr.time_to_live());
  EXPECT_EQ(boost::asio::ip::address_v4::from_string("8.8.8.8"), ipv4_hdr.source_address());

  EXPECT_EQ(icmp_header::echo_reply, icmp_hdr.type());
  EXPECT_EQ(0, icmp_hdr.code());
  EXPECT_EQ(0x1234, icmp_hdr.identifier());
  EXPECT_EQ(0x0001, icmp_hdr.sequence_number());
}

TEST(IcmpEchoReply, PayloadLengthCalculation) {
  // Total length 84 (20 IP header + 8 ICMP header + 56 payload)
  const std::string ip_raw = make_ipv4_header_bytes(64, 1, "8.8.8.8", "192.168.1.1", 84);

  std::stringstream ss(ip_raw);
  ipv4_header ipv4_hdr;
  ss >> ipv4_hdr;

  EXPECT_TRUE(ss.good());
  const std::size_t icmp_length = ipv4_hdr.total_length() - ipv4_hdr.header_length();
  EXPECT_EQ(64u, icmp_length);  // 8 byte ICMP header + 56 byte payload
}

// ============================================================================
// icmp_header overwrite behavior
// ============================================================================

TEST(IcmpHeader, OverwriteFieldsPreservesOtherFields) {
  icmp_header hdr;
  hdr.type(icmp_header::echo_request);
  hdr.code(0);
  hdr.identifier(0xAAAA);
  hdr.sequence_number(0xBBBB);

  // Overwrite only identifier
  hdr.identifier(0xCCCC);
  EXPECT_EQ(icmp_header::echo_request, hdr.type());
  EXPECT_EQ(0, hdr.code());
  EXPECT_EQ(0xCCCC, hdr.identifier());
  EXPECT_EQ(0xBBBB, hdr.sequence_number());
}

// ============================================================================
// ipv4_header address edge cases
// ============================================================================

TEST(Ipv4Header, ParseLoopbackAddress) {
  std::string raw = make_ipv4_header_bytes(64, 1, "127.0.0.1", "127.0.0.1");
  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.good());
  EXPECT_EQ(boost::asio::ip::address_v4::from_string("127.0.0.1"), hdr.source_address());
  EXPECT_EQ(boost::asio::ip::address_v4::from_string("127.0.0.1"), hdr.destination_address());
}

TEST(Ipv4Header, ParseBroadcastAddress) {
  const std::string raw = make_ipv4_header_bytes(1, 1, "255.255.255.255", "0.0.0.0");
  std::stringstream ss(raw);
  ipv4_header hdr;
  ss >> hdr;
  EXPECT_TRUE(ss.good());
  EXPECT_EQ(boost::asio::ip::address_v4::from_string("255.255.255.255"), hdr.source_address());
  EXPECT_EQ(boost::asio::ip::address_v4::from_string("0.0.0.0"), hdr.destination_address());
}

// ============================================================================
// pinger construction (requires no actual network)
// ============================================================================

TEST(Pinger, PingerSendsToResolvedEndpoint) {
  // Construct a pinger targeting localhost — constructor resolves the name
  // but does not send until ping() is called
  boost::asio::io_service io_service;
  result_container result;

  // Use "127.0.0.1" which resolves without DNS
  try {
    pinger p(io_service, result, "127.0.0.1", 1000, 42, "payload");
  } catch (const boost::system::system_error& e) {
    GTEST_SKIP() << "Skipping: unable to create ICMP socket or resolve address: " << e.what();
  } catch (const std::exception& e) {
    GTEST_SKIP() << "Skipping: pinger construction failed: " << e.what();
  }

  EXPECT_EQ("127.0.0.1", result.destination_);
  EXPECT_EQ("127.0.0.1", result.ip_);
  EXPECT_EQ(0u, result.num_send_);
  EXPECT_EQ(0u, result.num_replies_);
  EXPECT_EQ(0u, result.num_timeouts_);
}
