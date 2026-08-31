// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <net/check_mk/client/client_protocol.hpp>
#include <string>
#include <vector>

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

/// Copy a string into the protocol's inbound buffer and feed it via on_read.
void feed(check_mk::client::protocol& proto, const std::string& data) {
  auto& inbound = proto.get_inbound();
  ASSERT_GE(inbound.size(), data.size());
  std::copy(data.begin(), data.end(), inbound.begin());
  EXPECT_TRUE(proto.on_read(data.size()));
}

}  // namespace

// =============================================================================
// client::protocol — initial state
// =============================================================================

TEST(CheckMkClientProtocol, InitialState) {
  auto handler = std::make_shared<MockClientHandler>();
  check_mk::client::protocol proto(handler);

  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

TEST(CheckMkClientProtocol, BuffersAreAllocated) {
  auto handler = std::make_shared<MockClientHandler>();
  check_mk::client::protocol proto(handler);

  EXPECT_EQ(proto.get_inbound().size(), 40960u);
  EXPECT_EQ(proto.get_outbound().size(), 40960u);
}

// =============================================================================
// client::protocol — state transitions
// =============================================================================

TEST(CheckMkClientProtocol, OnConnectDoesNotWantDataYet) {
  auto handler = std::make_shared<MockClientHandler>();
  check_mk::client::protocol proto(handler);
  proto.on_connect();

  EXPECT_FALSE(proto.wants_data());
}

TEST(CheckMkClientProtocol, PrepareRequestWantsResponse) {
  auto handler = std::make_shared<MockClientHandler>();
  check_mk::client::protocol proto(handler);
  proto.on_connect();

  check_mk::client::protocol::request_type request;
  proto.prepare_request(request);

  EXPECT_TRUE(proto.wants_data());
  // The check_mk client never sends data - it only reads the agent output.
  EXPECT_FALSE(proto.has_data());
}

// =============================================================================
// client::protocol — reading data and building the response
// =============================================================================

TEST(CheckMkClientProtocol, OnReadAccumulatesIntoResponse) {
  auto handler = std::make_shared<MockClientHandler>();
  check_mk::client::protocol proto(handler);
  proto.on_connect();
  check_mk::client::protocol::request_type request;
  proto.prepare_request(request);

  feed(proto, "<<<check_mk>>>\nVersion: 1.2.3\n");

  check_mk::packet response = proto.get_response();
  ASSERT_EQ(response.section_list.size(), 1u);
  EXPECT_EQ(response.get_section(0).title, "check_mk");
  EXPECT_EQ(response.get_section(0).get_line(0).get_line(), "Version: 1.2.3");
}

TEST(CheckMkClientProtocol, OnReadAccumulatesAcrossMultipleReads) {
  auto handler = std::make_shared<MockClientHandler>();
  check_mk::client::protocol proto(handler);
  proto.on_connect();
  check_mk::client::protocol::request_type request;
  proto.prepare_request(request);

  feed(proto, "<<<check_mk>>>\nVer");
  feed(proto, "sion: 1.0\n<<<local>>>\nok\n");

  check_mk::packet response = proto.get_response();
  ASSERT_EQ(response.section_list.size(), 2u);
  EXPECT_EQ(response.get_section(0).get_line(0).get_line(), "Version: 1.0");
  EXPECT_EQ(response.get_section(1).title, "local");
}

TEST(CheckMkClientProtocol, GetTimeoutResponseIsEmptyPacket) {
  auto handler = std::make_shared<MockClientHandler>();
  check_mk::client::protocol proto(handler);
  check_mk::packet response = proto.get_timeout_response();
  EXPECT_TRUE(response.section_list.empty());
  EXPECT_TRUE(response.piggybacks.empty());
}

TEST(CheckMkClientProtocol, OnWriteReturnsTrue) {
  auto handler = std::make_shared<MockClientHandler>();
  check_mk::client::protocol proto(handler);
  EXPECT_TRUE(proto.on_write(0));
}

// =============================================================================
// client::protocol — read errors
// =============================================================================

TEST(CheckMkClientProtocol, EofEndsTheReadLoopWithoutError) {
  auto handler = std::make_shared<MockClientHandler>();
  check_mk::client::protocol proto(handler);
  proto.on_connect();
  check_mk::client::protocol::request_type request;
  proto.prepare_request(request);

  feed(proto, "<<<check_mk>>>\nVersion: 1.0\n");

  // EOF is how the agent signals "all data sent": no error is logged and the
  // protocol stops wanting data.
  EXPECT_TRUE(proto.on_read_error(boost::asio::error::eof));
  EXPECT_FALSE(proto.wants_data());
  EXPECT_TRUE(handler->error_msgs.empty());

  check_mk::packet response = proto.get_response();
  EXPECT_EQ(response.get_section(0).title, "check_mk");
}

TEST(CheckMkClientProtocol, OtherReadErrorsAreLogged) {
  auto handler = std::make_shared<MockClientHandler>();
  check_mk::client::protocol proto(handler);
  proto.on_connect();
  check_mk::client::protocol::request_type request;
  proto.prepare_request(request);

  EXPECT_TRUE(proto.on_read_error(boost::asio::error::connection_reset));
  ASSERT_EQ(handler->error_msgs.size(), 1u);
  EXPECT_NE(handler->error_msgs[0].find("Failed to receive MK data"), std::string::npos);
}
