// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <net/check_nt/packet.hpp>
#include <net/check_nt/server/handler.hpp>
#include <net/check_nt/server/protocol.hpp>
#include <stdexcept>
#include <string>
#include <vector>

// =============================================================================
// Mock handler for check_nt server protocol tests
// =============================================================================
namespace {

class MockCheckNtHandler : public check_nt::server::handler {
 public:
  std::vector<std::string> received_payloads;
  std::string response_ = "the response";
  bool throw_on_handle_ = false;
  mutable std::vector<std::string> debug_msgs;
  mutable std::vector<std::string> error_msgs;

  check_nt::packet handle(check_nt::packet packet) override {
    if (throw_on_handle_) throw std::runtime_error("boom");
    received_payloads.push_back(packet.get_payload());
    return check_nt::packet(response_);
  }
  check_nt::packet create_error(std::string msg) override { return check_nt::packet("ERROR: " + msg); }
  void log_debug(std::string, std::string, int, std::string msg) const override { debug_msgs.push_back(msg); }
  void log_error(std::string, std::string, int, std::string msg) const override { error_msgs.push_back(msg); }

  void set_allow_arguments(bool) override {}
  void set_allow_nasty_arguments(bool) override {}
  void set_perf_data(bool) override {}
  void set_password(std::string password) override { password_ = password; }
  std::string get_password() const override { return password_; }

 private:
  std::string password_;
};

std::string outbound_of(const std::shared_ptr<check_nt::read_protocol> &proto) {
  const std::vector<char> data = proto->get_outbound();
  return std::string(data.begin(), data.end());
}

bool feed(const std::shared_ptr<check_nt::read_protocol> &proto, const std::string &chunk) {
  std::vector<char> buf(chunk.begin(), chunk.end());
  return proto->on_read(buf.data(), buf.data() + buf.size());
}

}  // namespace

// =============================================================================
// read_protocol — state machine
// =============================================================================

TEST(CheckNtServerProtocol, InitialState) {
  MockCheckNtHandler handler;
  socket_helpers::connection_info info;
  auto proto = check_nt::read_protocol::create(info, &handler);

  EXPECT_FALSE(proto->wants_data());
  EXPECT_FALSE(proto->has_data());
}

TEST(CheckNtServerProtocol, OnConnectWantsData) {
  MockCheckNtHandler handler;
  socket_helpers::connection_info info;
  auto proto = check_nt::read_protocol::create(info, &handler);

  EXPECT_TRUE(proto->on_connect());
  EXPECT_TRUE(proto->wants_data());
  EXPECT_FALSE(proto->has_data());
}

// The real nagios-plugins check_nt sends its request with no terminator and
// waits: one read chunk is one request and the response must become
// available immediately.
TEST(CheckNtServerProtocol, RequestWithoutNewlineIsHandledAtEndOfRead) {
  MockCheckNtHandler handler;
  socket_helpers::connection_info info;
  auto proto = check_nt::read_protocol::create(info, &handler);
  proto->on_connect();

  EXPECT_TRUE(feed(proto, "password&1"));

  ASSERT_EQ(handler.received_payloads.size(), 1u);
  EXPECT_EQ(handler.received_payloads[0], "password&1");
  EXPECT_TRUE(proto->has_data());
  EXPECT_FALSE(proto->wants_data());
  EXPECT_EQ(outbound_of(proto), "the response");
}

TEST(CheckNtServerProtocol, NewlineTerminatedRequestIsHandledAtTheNewline) {
  MockCheckNtHandler handler;
  socket_helpers::connection_info info;
  auto proto = check_nt::read_protocol::create(info, &handler);
  proto->on_connect();

  EXPECT_TRUE(feed(proto, "password&3\n"));

  ASSERT_EQ(handler.received_payloads.size(), 1u);
  // The terminator stays in the payload; the server's request parsing
  // strips trailing line breaks.
  EXPECT_EQ(handler.received_payloads[0], "password&3\n");
  EXPECT_TRUE(proto->has_data());
}

TEST(CheckNtServerProtocol, HandlerExceptionBecomesAnErrorResponse) {
  MockCheckNtHandler handler;
  handler.throw_on_handle_ = true;
  socket_helpers::connection_info info;
  auto proto = check_nt::read_protocol::create(info, &handler);
  proto->on_connect();

  EXPECT_TRUE(feed(proto, "password&1"));

  EXPECT_TRUE(proto->has_data());
  EXPECT_EQ(outbound_of(proto), "ERROR: Exception processing request: boom");
}

TEST(CheckNtServerProtocol, OnWriteFinishesTheExchange) {
  MockCheckNtHandler handler;
  socket_helpers::connection_info info;
  auto proto = check_nt::read_protocol::create(info, &handler);
  proto->on_connect();
  feed(proto, "password&1");

  proto->on_write();

  EXPECT_FALSE(proto->wants_data());
  EXPECT_FALSE(proto->has_data());
}

TEST(CheckNtServerProtocol, GetInfoReturnsConnectionInfo) {
  MockCheckNtHandler handler;
  socket_helpers::connection_info info;
  info.ssl.enabled = false;
  auto proto = check_nt::read_protocol::create(info, &handler);

  EXPECT_EQ(proto->get_info().ssl.enabled, false);
}
