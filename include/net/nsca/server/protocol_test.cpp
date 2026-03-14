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
#include <net/nsca/server/handler.hpp>
#include <net/nsca/server/protocol.hpp>
#include <string>
#include <vector>

// =============================================================================
// Mock handler for NSCA server protocol tests
// =============================================================================
namespace {

class MockNscaHandler : public nsca::server::handler {
 public:
  unsigned int payload_length_ = 512;
  int encryption_ = 0;  // no encryption (XOR with no password = identity)
  std::string password_;
  std::vector<nsca::packet> received_packets;
  mutable std::vector<std::string> debug_msgs;
  mutable std::vector<std::string> error_msgs;

  void handle(nsca::packet packet) override { received_packets.push_back(packet); }
  void log_debug(std::string, std::string, int, std::string msg) const override { debug_msgs.push_back(msg); }
  void log_error(std::string, std::string, int, std::string msg) const override { error_msgs.push_back(msg); }
  unsigned int get_payload_length() override { return payload_length_; }
  int get_encryption() override { return encryption_; }
  std::string get_password() override { return password_; }
};

}  // namespace

// =============================================================================
// read_protocol — initial state
// =============================================================================

TEST(NscaServerProtocol, InitialState) {
  MockNscaHandler handler;
  socket_helpers::connection_info info;
  auto proto = nsca::read_protocol::create(info, &handler);

  EXPECT_FALSE(proto->wants_data());
  EXPECT_FALSE(proto->has_data());
}

// =============================================================================
// read_protocol — on_connect
// =============================================================================

TEST(NscaServerProtocol, OnConnectSetsConnected) {
  MockNscaHandler handler;
  socket_helpers::connection_info info;
  auto proto = nsca::read_protocol::create(info, &handler);

  EXPECT_TRUE(proto->on_connect());
  // After connect, protocol has IV data to send (state = connected)
  EXPECT_TRUE(proto->has_data());
  EXPECT_FALSE(proto->wants_data());
}

// =============================================================================
// read_protocol — get_outbound returns IV data
// =============================================================================

TEST(NscaServerProtocol, GetOutboundReturnsIvData) {
  MockNscaHandler handler;
  socket_helpers::connection_info info;
  auto proto = nsca::read_protocol::create(info, &handler);
  proto->on_connect();

  std::string outbound = proto->get_outbound();
  EXPECT_EQ(outbound.size(), nsca::length::iv::get_packet_length());
}

// =============================================================================
// read_protocol — on_write transitions to sent_iv (wants_data)
// =============================================================================

TEST(NscaServerProtocol, OnWriteTransitionsToSentIv) {
  MockNscaHandler handler;
  socket_helpers::connection_info info;
  auto proto = nsca::read_protocol::create(info, &handler);
  proto->on_connect();

  EXPECT_TRUE(proto->on_write());
  // After writing IV, protocol wants data from client
  EXPECT_TRUE(proto->wants_data());
  EXPECT_FALSE(proto->has_data());
}

// =============================================================================
// read_protocol — get_info
// =============================================================================

TEST(NscaServerProtocol, GetInfoReturnsConnectionInfo) {
  MockNscaHandler handler;
  socket_helpers::connection_info info;
  info.ssl.enabled = false;
  auto proto = nsca::read_protocol::create(info, &handler);

  auto retrieved = proto->get_info();
  EXPECT_EQ(retrieved.ssl.enabled, false);
}

