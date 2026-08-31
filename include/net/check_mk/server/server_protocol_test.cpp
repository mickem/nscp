// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <memory>
#include <net/check_mk/server/server_protocol.hpp>
#include <string>
#include <vector>

// =============================================================================
// Mock handler for server protocol tests
// =============================================================================
namespace {

class MockHandler : public check_mk::server::handler {
 public:
  check_mk::packet response;
  mutable std::vector<std::string> debug_msgs;
  mutable std::vector<std::string> error_msgs;
  mutable int process_calls = 0;

  check_mk::packet process() override {
    process_calls++;
    return response;
  }

  void log_debug(std::string, std::string, int, std::string msg) const override { debug_msgs.push_back(msg); }
  void log_error(std::string, std::string, int, std::string msg) const override { error_msgs.push_back(msg); }
};

std::shared_ptr<MockHandler> make_handler() {
  auto handler = std::make_shared<MockHandler>();
  check_mk::packet::section s("check_mk");
  s.push("Version: 1.2.3");
  handler->response.add_section(s);
  return handler;
}

}  // namespace

// =============================================================================
// read_protocol — construction / initial state
// =============================================================================

TEST(CheckMkServerProtocol, InitialState) {
  auto handler = make_handler();
  socket_helpers::connection_info info;
  check_mk::read_protocol proto(info, handler);

  // The check_mk server never reads: it pushes the agent data on connect.
  EXPECT_FALSE(proto.wants_data());
  EXPECT_FALSE(proto.has_data());
  EXPECT_EQ(handler->process_calls, 0);
}

TEST(CheckMkServerProtocol, CreateReturnsProtocol) {
  auto handler = make_handler();
  socket_helpers::connection_info info;
  auto proto = check_mk::read_protocol::create(info, handler);
  ASSERT_TRUE(proto);
  EXPECT_FALSE(proto->has_data());
}

// =============================================================================
// read_protocol — on_connect builds the outbound payload
// =============================================================================

TEST(CheckMkServerProtocol, OnConnectRendersHandlerPacket) {
  auto handler = make_handler();
  socket_helpers::connection_info info;
  check_mk::read_protocol proto(info, handler);

  EXPECT_TRUE(proto.on_connect());
  EXPECT_EQ(handler->process_calls, 1);
  EXPECT_TRUE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());

  std::vector<char> outbound = proto.get_outbound();
  EXPECT_EQ(std::string(outbound.begin(), outbound.end()), "<<<check_mk>>>\nVersion: 1.2.3\n");
}

TEST(CheckMkServerProtocol, OnConnectWithEmptyPacketYieldsEmptyOutbound) {
  auto handler = std::make_shared<MockHandler>();
  socket_helpers::connection_info info;
  check_mk::read_protocol proto(info, handler);

  EXPECT_TRUE(proto.on_connect());
  EXPECT_TRUE(proto.get_outbound().empty());
  // has_data reflects the state machine, not the payload size.
  EXPECT_TRUE(proto.has_data());
}

// =============================================================================
// read_protocol — write completes the exchange
// =============================================================================

TEST(CheckMkServerProtocol, OnWriteTransitionsToDone) {
  auto handler = make_handler();
  socket_helpers::connection_info info;
  check_mk::read_protocol proto(info, handler);
  proto.on_connect();

  EXPECT_TRUE(proto.has_data());
  proto.on_write();
  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

TEST(CheckMkServerProtocol, OnReadIsIgnoredButReturnsTrue) {
  auto handler = make_handler();
  socket_helpers::connection_info info;
  check_mk::read_protocol proto(info, handler);
  char buf[4] = {0};
  EXPECT_TRUE(proto.on_read(buf, buf + sizeof(buf)));
}

// =============================================================================
// read_protocol — connection info and logging
// =============================================================================

TEST(CheckMkServerProtocol, GetInfoReturnsConnectionInfo) {
  auto handler = make_handler();
  socket_helpers::connection_info info;
  info.timeout = 30;
  info.port_ = "6556";

  check_mk::read_protocol proto(info, handler);
  socket_helpers::connection_info returned = proto.get_info();
  EXPECT_EQ(returned.timeout, 30u);
  EXPECT_EQ(returned.get_port(), "6556");
}

TEST(CheckMkServerProtocol, LogDebugDelegatesToHandler) {
  auto handler = make_handler();
  socket_helpers::connection_info info;
  check_mk::read_protocol proto(info, handler);
  proto.log_debug("file.cpp", 42, "debug message");
  ASSERT_EQ(handler->debug_msgs.size(), 1u);
  EXPECT_EQ(handler->debug_msgs[0], "debug message");
}

TEST(CheckMkServerProtocol, LogErrorDelegatesToHandler) {
  auto handler = make_handler();
  socket_helpers::connection_info info;
  check_mk::read_protocol proto(info, handler);
  proto.log_error("file.cpp", 42, "error message");
  ASSERT_EQ(handler->error_msgs.size(), 1u);
  EXPECT_EQ(handler->error_msgs[0], "error message");
}
