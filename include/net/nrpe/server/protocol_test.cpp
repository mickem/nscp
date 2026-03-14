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

#include <net/nrpe/server/handler.hpp>
#include <net/nrpe/server/protocol.hpp>
#include <string>
#include <vector>

using namespace nrpe;

// =============================================================================
// Mock handler for server protocol tests
// =============================================================================
namespace {

class MockHandler : public server::handler {
 public:
  unsigned int payload_length = 1024;
  std::list<packet> response_packets;
  packet error_packet = packet::create_response(data::version2, 3, "error", 1024);
  mutable std::vector<std::string> debug_msgs;
  mutable std::vector<std::string> error_msgs;

  std::list<packet> handle(packet) override { return response_packets; }

  void log_debug(std::string, std::string, int, std::string msg) const override { debug_msgs.push_back(msg); }

  void log_error(std::string, std::string, int, std::string msg) const override { error_msgs.push_back(msg); }

  packet create_error(std::string msg) override { return packet::create_response(data::version2, 3, msg, payload_length); }

  unsigned int get_payload_length() override { return payload_length; }
};

}  // namespace

// =============================================================================
// read_protocol — initial state
// =============================================================================

TEST(NrpeServerProtocol, InitialState) {
  MockHandler handler;
  socket_helpers::connection_info info;
  read_protocol proto(info, &handler);

  EXPECT_FALSE(proto.wants_data());
  EXPECT_FALSE(proto.has_data());
}

// =============================================================================
// read_protocol — on_connect
// =============================================================================

TEST(NrpeServerProtocol, OnConnectSetsConnected) {
  MockHandler handler;
  socket_helpers::connection_info info;
  read_protocol proto(info, &handler);

  EXPECT_TRUE(proto.on_connect());
  EXPECT_TRUE(proto.wants_data());
  EXPECT_FALSE(proto.has_data());
}

// =============================================================================
// read_protocol — on_read with a complete valid packet
// =============================================================================

TEST(NrpeServerProtocol, OnReadWithValidPacket) {
  MockHandler handler;
  handler.response_packets.push_back(packet::create_response(data::version2, 0, "OK", 1024));

  socket_helpers::connection_info info;
  read_protocol proto(info, &handler);
  proto.on_connect();

  // Build a valid query packet
  packet query = packet::make_request("check_test", 1024, 2);
  std::vector<char> buf = query.get_buffer();

  bool result = proto.on_read(buf.data(), buf.data() + buf.size());
  EXPECT_TRUE(result);

  // After reading a complete packet, protocol should have data to send
  EXPECT_TRUE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

// =============================================================================
// read_protocol — get_outbound returns response data
// =============================================================================

TEST(NrpeServerProtocol, GetOutboundReturnsResponseData) {
  MockHandler handler;
  handler.response_packets.push_back(packet::create_response(data::version2, 0, "OK", 1024));

  socket_helpers::connection_info info;
  read_protocol proto(info, &handler);
  proto.on_connect();

  packet query = packet::make_request("check_test", 1024, 2);
  std::vector<char> buf = query.get_buffer();
  proto.on_read(buf.data(), buf.data() + buf.size());

  std::vector<char> outbound = proto.get_outbound();
  EXPECT_FALSE(outbound.empty());
}

// =============================================================================
// read_protocol — on_write with single response transitions to done
// =============================================================================

TEST(NrpeServerProtocol, OnWriteSingleResponseGoesToDone) {
  MockHandler handler;
  handler.response_packets.push_back(packet::create_response(data::version2, 0, "OK", 1024));

  socket_helpers::connection_info info;
  read_protocol proto(info, &handler);
  proto.on_connect();

  packet query = packet::make_request("check_test", 1024, 2);
  std::vector<char> buf = query.get_buffer();
  proto.on_read(buf.data(), buf.data() + buf.size());

  // After on_read with one response: state = last_packet
  EXPECT_TRUE(proto.has_data());

  proto.on_write();

  // After writing the only response: state = done
  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

// =============================================================================
// read_protocol — on_write with multiple responses
// =============================================================================

TEST(NrpeServerProtocol, OnWriteMultipleResponses) {
  MockHandler handler;
  handler.response_packets.push_back(packet::create_more_response(0, "part1", 1024));
  handler.response_packets.push_back(packet::create_response(data::version2, 0, "part2", 1024));

  socket_helpers::connection_info info;
  read_protocol proto(info, &handler);
  proto.on_connect();

  packet query = packet::make_request("check_test", 1024, 2);
  std::vector<char> buf = query.get_buffer();
  proto.on_read(buf.data(), buf.data() + buf.size());

  // First response queued: state = has_more (because more responses follow)
  EXPECT_TRUE(proto.has_data());

  proto.on_write();

  // Second (last) response queued: state = last_packet
  EXPECT_TRUE(proto.has_data());

  proto.on_write();

  // All responses sent: state = done
  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

// =============================================================================
// read_protocol — has_more_response
// =============================================================================

TEST(NrpeServerProtocol, HasMoreResponseTracksPending) {
  MockHandler handler;
  handler.response_packets.push_back(packet::create_more_response(0, "first", 1024));
  handler.response_packets.push_back(packet::create_response(data::version2, 0, "last", 1024));

  socket_helpers::connection_info info;
  read_protocol proto(info, &handler);
  proto.on_connect();

  packet query = packet::make_request("check_test", 1024, 2);
  std::vector<char> buf = query.get_buffer();
  proto.on_read(buf.data(), buf.data() + buf.size());

  // After first queue_next: one response remains
  EXPECT_TRUE(proto.has_more_response());

  proto.on_write();  // queues the last packet

  // After writing, last response is current, no more pending
  EXPECT_FALSE(proto.has_more_response());
}

// =============================================================================
// read_protocol — get_info
// =============================================================================

TEST(NrpeServerProtocol, GetInfoReturnsConnectionInfo) {
  MockHandler handler;
  socket_helpers::connection_info info;
  info.timeout = 60;
  info.port_ = "5666";

  read_protocol proto(info, &handler);
  auto returned = proto.get_info();
  EXPECT_EQ(returned.timeout, 60u);
  EXPECT_EQ(returned.get_port(), "5666");
}

// =============================================================================
// read_protocol — logging
// =============================================================================

TEST(NrpeServerProtocol, LogDebugDelegatesToHandler) {
  MockHandler handler;
  socket_helpers::connection_info info;
  read_protocol proto(info, &handler);
  proto.log_debug("file.cpp", 42, "debug message");
  ASSERT_EQ(handler.debug_msgs.size(), 1u);
  EXPECT_EQ(handler.debug_msgs[0], "debug message");
}

TEST(NrpeServerProtocol, LogErrorDelegatesToHandler) {
  MockHandler handler;
  socket_helpers::connection_info info;
  read_protocol proto(info, &handler);
  proto.log_error("file.cpp", 42, "error message");
  ASSERT_EQ(handler.error_msgs.size(), 1u);
  EXPECT_EQ(handler.error_msgs[0], "error message");
}

