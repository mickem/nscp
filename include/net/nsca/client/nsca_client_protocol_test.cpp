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

#include <boost/make_shared.hpp>
#include <net/nsca/client/nsca_client_protocol.hpp>
#include <net/nsca/nsca_packet.hpp>
#include <string>
#include <vector>

// =============================================================================
// Mock handler for NSCA client protocol tests
// =============================================================================
namespace {

class MockNscaClientHandler {
 public:
  std::string password_ = "";
  int encryption_ = 0;  // ENCRYPT_NONE

  std::string get_password() const { return password_; }
  int get_encryption() const { return encryption_; }
};

/// Build an IV packet buffer suitable for on_read().
std::string make_iv_buffer() {
  std::string iv(nsca::length::iv::get_payload_length(), '\0');
  for (unsigned i = 0; i < iv.size(); ++i) iv[i] = static_cast<char>(i % 256);
  uint32_t time_val = 1000;
  nsca::iv_packet iv_pkt(iv, time_val);
  return iv_pkt.get_buffer();
}

}  // namespace

typedef nsca::client::protocol<MockNscaClientHandler> TestProtocol;

// =============================================================================
// initial state
// =============================================================================

TEST(NscaClientProtocol, InitialState) {
  auto handler = boost::make_shared<MockNscaClientHandler>();
  TestProtocol proto(handler);

  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

// =============================================================================
// on_connect
// =============================================================================

TEST(NscaClientProtocol, OnConnectTransitionsToWantsData) {
  auto handler = boost::make_shared<MockNscaClientHandler>();
  TestProtocol proto(handler);
  proto.on_connect();

  // After connect, protocol wants IV data from server
  EXPECT_TRUE(proto.wants_data());
  EXPECT_FALSE(proto.has_data());
}

// =============================================================================
// get_inbound returns IV-sized buffer
// =============================================================================

TEST(NscaClientProtocol, GetInboundReturnsIvSizedBuffer) {
  auto handler = boost::make_shared<MockNscaClientHandler>();
  TestProtocol proto(handler);
  proto.on_connect();

  auto& inbound = proto.get_inbound();
  EXPECT_EQ(inbound.size(), nsca::length::iv::get_packet_length());
}

// =============================================================================
// on_read — processes IV and transitions to got_iv
// =============================================================================

TEST(NscaClientProtocol, OnReadProcessesIvPacket) {
  auto handler = boost::make_shared<MockNscaClientHandler>();
  TestProtocol proto(handler);
  proto.on_connect();

  // Fill inbound with a valid IV packet
  std::string iv_buf = make_iv_buffer();
  auto& inbound = proto.get_inbound();
  std::copy(iv_buf.begin(), iv_buf.end(), inbound.begin());

  bool result = proto.on_read(iv_buf.size());
  EXPECT_TRUE(result);

  // After reading IV, protocol has data (got_iv) but doesn't want more yet
  EXPECT_TRUE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}


// =============================================================================
// prepare_request from sent_request transitions to has_request
// =============================================================================

TEST(NscaClientProtocol, PrepareRequestFromSentRequestTransitions) {
  auto handler = boost::make_shared<MockNscaClientHandler>();
  TestProtocol proto(handler);
  proto.on_connect();

  std::string iv_buf = make_iv_buffer();
  auto& inbound = proto.get_inbound();
  std::copy(iv_buf.begin(), iv_buf.end(), inbound.begin());
  proto.on_read(iv_buf.size());

  nsca::packet pkt1("host1");
  pkt1.service = "svc1";
  pkt1.result = "OK";
  pkt1.code = 0;
  proto.prepare_request(pkt1);
  proto.get_outbound();
  proto.on_write(100);

  // Now state is sent_request; prepare another request
  nsca::packet pkt2("host2");
  pkt2.service = "svc2";
  pkt2.result = "WARN";
  pkt2.code = 1;
  proto.prepare_request(pkt2);

  EXPECT_TRUE(proto.has_data());
}

// =============================================================================
// get_outbound returns packet-sized buffer
// =============================================================================

TEST(NscaClientProtocol, GetOutboundReturnsPacketSizedBuffer) {
  auto handler = boost::make_shared<MockNscaClientHandler>();
  TestProtocol proto(handler);
  proto.on_connect();

  std::string iv_buf = make_iv_buffer();
  auto& inbound = proto.get_inbound();
  std::copy(iv_buf.begin(), iv_buf.end(), inbound.begin());
  proto.on_read(iv_buf.size());

  nsca::packet pkt("testhost");
  pkt.service = "testsvc";
  pkt.result = "ALL OK";
  pkt.code = 0;
  proto.prepare_request(pkt);

  auto& outbound = proto.get_outbound();
  EXPECT_EQ(outbound.size(), pkt.get_packet_length());
}

// =============================================================================
// on_write transitions to sent_request
// =============================================================================

TEST(NscaClientProtocol, OnWriteTransitionsToSentRequest) {
  auto handler = boost::make_shared<MockNscaClientHandler>();
  TestProtocol proto(handler);
  proto.on_connect();

  std::string iv_buf = make_iv_buffer();
  auto& inbound = proto.get_inbound();
  std::copy(iv_buf.begin(), iv_buf.end(), inbound.begin());
  proto.on_read(iv_buf.size());

  nsca::packet pkt("host");
  pkt.service = "svc";
  pkt.result = "OK";
  pkt.code = 0;
  proto.prepare_request(pkt);
  proto.get_outbound();

  bool result = proto.on_write(100);
  EXPECT_TRUE(result);

  // After writing, no more data to send and no data to read
  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

// =============================================================================
// get_timeout_response
// =============================================================================

TEST(NscaClientProtocol, GetTimeoutResponseReturnsFalse) {
  auto handler = boost::make_shared<MockNscaClientHandler>();
  TestProtocol proto(handler);

  EXPECT_FALSE(proto.get_timeout_response());
}

// =============================================================================
// get_response
// =============================================================================

TEST(NscaClientProtocol, GetResponseReturnsTrue) {
  auto handler = boost::make_shared<MockNscaClientHandler>();
  TestProtocol proto(handler);

  EXPECT_TRUE(proto.get_response());
}

// =============================================================================
// on_read_error
// =============================================================================

TEST(NscaClientProtocol, OnReadErrorReturnsFalse) {
  auto handler = boost::make_shared<MockNscaClientHandler>();
  TestProtocol proto(handler);

  boost::system::error_code ec = boost::asio::error::connection_reset;
  EXPECT_FALSE(proto.on_read_error(ec));
}

// =============================================================================
// Full send cycle: connect → read IV → prepare → outbound → write
// =============================================================================

TEST(NscaClientProtocol, FullSendCycle) {
  auto handler = boost::make_shared<MockNscaClientHandler>();
  TestProtocol proto(handler);

  // 1. connect
  proto.on_connect();
  EXPECT_TRUE(proto.wants_data());

  // 2. receive IV
  std::string iv_buf = make_iv_buffer();
  auto& inbound = proto.get_inbound();
  std::copy(iv_buf.begin(), iv_buf.end(), inbound.begin());
  proto.on_read(iv_buf.size());
  EXPECT_TRUE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());

  // 3. prepare a request
  nsca::packet pkt("myhost");
  pkt.service = "disk";
  pkt.result = "OK - 50% free";
  pkt.code = 0;
  proto.prepare_request(pkt);
  //EXPECT_TRUE(proto.has_data());

  // 4. get outbound buffer and write
  auto& outbound = proto.get_outbound();
  EXPECT_FALSE(outbound.empty());
  proto.on_write(outbound.size());

  // 5. done
  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
  EXPECT_TRUE(proto.get_response());
}

// =============================================================================
// Encryption with XOR (encryption = 1)
// =============================================================================

TEST(NscaClientProtocol, EncryptionXorProducesEncryptedOutbound) {
  auto handler = boost::make_shared<MockNscaClientHandler>();
  handler->encryption_ = 1;  // ENCRYPT_XOR
  handler->password_ = "secret";
  TestProtocol proto(handler);

  proto.on_connect();

  std::string iv_buf = make_iv_buffer();
  auto& inbound = proto.get_inbound();
  std::copy(iv_buf.begin(), iv_buf.end(), inbound.begin());
  proto.on_read(iv_buf.size());

  nsca::packet pkt("host");
  pkt.service = "svc";
  pkt.result = "OK";
  pkt.code = 0;
  proto.prepare_request(pkt);

  auto& outbound = proto.get_outbound();
  EXPECT_EQ(outbound.size(), pkt.get_packet_length());

  // With XOR encryption, the buffer should differ from an unencrypted one
  auto handler2 = boost::make_shared<MockNscaClientHandler>();
  handler2->encryption_ = 0;  // ENCRYPT_NONE
  TestProtocol proto2(handler2);
  proto2.on_connect();
  auto& inbound2 = proto2.get_inbound();
  std::copy(iv_buf.begin(), iv_buf.end(), inbound2.begin());
  proto2.on_read(iv_buf.size());
  proto2.prepare_request(pkt);
  auto& outbound2 = proto2.get_outbound();

  EXPECT_NE(outbound, outbound2);
}

