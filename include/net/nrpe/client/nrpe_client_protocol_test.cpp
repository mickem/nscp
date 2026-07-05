// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <memory>
#include <net/nrpe/client/nrpe_client_protocol.hpp>
#include <net/nrpe/packet.hpp>
#include <string>
#include <vector>

using namespace nrpe;

// =============================================================================
// Mock client handler
// =============================================================================
namespace {

class MockClientHandler : public socket_helpers::client::client_handler {
 public:
  mutable std::vector<std::string> debug_msgs;
  mutable std::vector<std::string> error_msgs;

  void log_debug(std::string, int, std::string msg) const override { debug_msgs.push_back(msg); }
  void log_error(std::string, int, std::string msg) const override { error_msgs.push_back(msg); }
  std::string expand_path(std::string path) override { return path; }
};

}  // namespace

// =============================================================================
// client::protocol — initial state
// =============================================================================

TEST(NrpeClientProtocol, InitialState) {
  auto handler = std::make_shared<MockClientHandler>();
  client::protocol proto(handler);

  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

// =============================================================================
// client::protocol — on_connect
// =============================================================================

TEST(NrpeClientProtocol, OnConnectSetsConnected) {
  auto handler = std::make_shared<MockClientHandler>();
  client::protocol proto(handler);
  proto.on_connect();

  // After connect, still no data to send or receive until prepare_request
  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

// =============================================================================
// client::protocol — prepare_request
// =============================================================================

TEST(NrpeClientProtocol, PrepareRequestSetsHasData) {
  auto handler = std::make_shared<MockClientHandler>();
  client::protocol proto(handler);
  proto.on_connect();

  packet request = packet::make_request("check_cpu", 1024, 2);
  proto.prepare_request(request);

  EXPECT_TRUE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

TEST(NrpeClientProtocol, PrepareRequestPopulatesOutbound) {
  auto handler = std::make_shared<MockClientHandler>();
  client::protocol proto(handler);
  proto.on_connect();

  packet request = packet::make_request("check_disk", 1024, 2);
  proto.prepare_request(request);

  auto& outbound = proto.get_outbound();
  EXPECT_FALSE(outbound.empty());
}

// =============================================================================
// client::protocol — on_write transitions to wants_data
// =============================================================================

TEST(NrpeClientProtocol, OnWriteTransitionsToWantsData) {
  auto handler = std::make_shared<MockClientHandler>();
  client::protocol proto(handler);
  proto.on_connect();

  packet request = packet::make_request("check_cpu", 1024, 2);
  proto.prepare_request(request);

  EXPECT_TRUE(proto.has_data());
  proto.on_write(100);

  EXPECT_FALSE(proto.has_data());
  EXPECT_TRUE(proto.wants_data());
}

// =============================================================================
// client::protocol — on_read with normal response
// =============================================================================

TEST(NrpeClientProtocol, OnReadNormalResponseTransitionsToConnected) {
  auto handler = std::make_shared<MockClientHandler>();
  client::protocol proto(handler);
  proto.on_connect();

  packet request = packet::make_request("check_cpu", 1024, 2);
  proto.prepare_request(request);
  proto.on_write(100);

  // Simulate receiving a response — populate inbound with a response packet
  packet response = packet::create_response(data::version2, 0, "OK", 1024);
  std::vector<char> resp_buf = response.get_buffer();
  auto& inbound = proto.get_inbound();
  inbound = resp_buf;

  bool result = proto.on_read(inbound.size());
  EXPECT_TRUE(result);

  // After reading a responsePacket, state goes back to connected
  EXPECT_FALSE(proto.wants_data());
  EXPECT_FALSE(proto.has_data());
}

TEST(NrpeClientProtocol, OnReadMoreResponseKeepsWantingData) {
  auto handler = std::make_shared<MockClientHandler>();
  client::protocol proto(handler);
  proto.on_connect();

  packet request = packet::make_request("check_cpu", 1024, 2);
  proto.prepare_request(request);
  proto.on_write(100);

  // Simulate receiving a moreResponsePacket
  packet more_resp = packet::create_more_response(0, "partial", 1024);
  std::vector<char> resp_buf = more_resp.get_buffer();
  auto& inbound = proto.get_inbound();
  inbound = resp_buf;

  bool result = proto.on_read(inbound.size());
  EXPECT_TRUE(result);

  // After moreResponsePacket, state = has_more -> still wants data
  EXPECT_TRUE(proto.wants_data());
}

// =============================================================================
// client::protocol — get_response
// =============================================================================

TEST(NrpeClientProtocol, GetResponseReturnsAccumulatedPackets) {
  auto handler = std::make_shared<MockClientHandler>();
  client::protocol proto(handler);
  proto.on_connect();

  packet request = packet::make_request("check_cpu", 1024, 2);
  proto.prepare_request(request);
  proto.on_write(100);

  // Read one response
  packet response = packet::create_response(data::version2, 0, "OK", 1024);
  std::vector<char> resp_buf = response.get_buffer();
  auto& inbound = proto.get_inbound();
  inbound = resp_buf;
  proto.on_read(inbound.size());

  auto responses = proto.get_response();
  EXPECT_EQ(responses.size(), 1u);
  EXPECT_EQ(responses.front().getPayload(), "OK");
}

// =============================================================================
// client::protocol — get_timeout_response
// =============================================================================

TEST(NrpeClientProtocol, GetTimeoutResponseReturnsUnknown) {
  auto handler = std::make_shared<MockClientHandler>();
  client::protocol proto(handler);

  auto resp = proto.get_timeout_response();
  EXPECT_EQ(resp.size(), 1u);
  EXPECT_EQ(resp.front().getPayload(), "Failed to read data");
}

// =============================================================================
// client::protocol — on_read_error
// =============================================================================

TEST(NrpeClientProtocol, OnReadErrorInConnectedStateReturnsTrue) {
  auto handler = std::make_shared<MockClientHandler>();
  client::protocol proto(handler);
  proto.on_connect();

  packet request = packet::make_request("check_cpu", 1024, 2);
  proto.prepare_request(request);
  proto.on_write(100);

  // Simulate reading a normal response to get back to connected state
  packet response = packet::create_response(data::version2, 0, "OK", 1024);
  std::vector<char> resp_buf = response.get_buffer();
  auto& inbound = proto.get_inbound();
  inbound = resp_buf;
  proto.on_read(inbound.size());

  // Now in connected state — read error is acceptable (end of responses)
  boost::system::error_code ec = boost::asio::error::eof;
  EXPECT_TRUE(proto.on_read_error(ec));
}

TEST(NrpeClientProtocol, OnReadErrorInNonConnectedStateReturnsFalse) {
  auto handler = std::make_shared<MockClientHandler>();
  client::protocol proto(handler);
  proto.on_connect();

  packet request = packet::make_request("check_cpu", 1024, 2);
  proto.prepare_request(request);
  proto.on_write(100);

  // In sent_response state — read error is a real error
  boost::system::error_code ec = boost::asio::error::eof;
  EXPECT_FALSE(proto.on_read_error(ec));
}

// =============================================================================
// client::protocol — full request/response cycle
// =============================================================================

TEST(NrpeClientProtocol, FullCycle) {
  auto handler = std::make_shared<MockClientHandler>();
  client::protocol proto(handler);

  // 1. Connect
  proto.on_connect();
  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());

  // 2. Prepare request
  packet request = packet::make_request("check_disk", 1024, 2);
  proto.prepare_request(request);
  EXPECT_TRUE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());

  // 3. Write request
  proto.on_write(proto.get_outbound().size());
  EXPECT_FALSE(proto.has_data());
  EXPECT_TRUE(proto.wants_data());

  // 4. Read response
  packet response = packet::create_response(data::version2, 0, "OK - disk fine", 1024);
  std::vector<char> resp_buf = response.get_buffer();
  proto.get_inbound() = resp_buf;
  proto.on_read(resp_buf.size());
  EXPECT_FALSE(proto.wants_data());
  EXPECT_FALSE(proto.has_data());

  // 5. Get result
  auto responses = proto.get_response();
  ASSERT_EQ(responses.size(), 1u);
  EXPECT_EQ(responses.front().getPayload(), "OK - disk fine");
  EXPECT_EQ(responses.front().getResult(), 0);
}
