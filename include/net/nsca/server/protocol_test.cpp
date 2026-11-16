// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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

// =============================================================================
// read_protocol — on_read: a whole client exchange
// =============================================================================

namespace {

// What an NSCA client does with the IV packet the server sent: encrypt a
// result with the shared password and that IV. `skew` moves the packet's
// timestamp away from now.
std::string client_packet(const std::string &iv_packet_bytes, const std::string &password, const int method, const int skew,
                          const unsigned int payload_length = 512) {
  nsca::packet p("web01", payload_length, skew);
  p.service = "disk";
  p.result = "CRITICAL: C: 99% used";
  p.code = 2;
  std::string buffer = p.get_buffer();
  nscp::encryption::engine e;
  e.encrypt_init(password, method, iv_packet_bytes.substr(0, nsca::length::iv::get_payload_length()));
  e.encrypt_buffer(buffer);
  return buffer;
}

struct Exchange {
  MockNscaHandler handler;
  std::shared_ptr<nsca::read_protocol> proto;
  explicit Exchange(const int method, const std::string &password = "secret") {
    handler.encryption_ = method;
    handler.password_ = password;
    proto = nsca::read_protocol::create(socket_helpers::connection_info(), &handler);
    proto->on_connect();
    proto->on_write();
  }
  bool feed(std::string data) { return proto->on_read(&data[0], &data[0] + data.size()); }
  bool any_error_contains(const std::string &needle) const {
    for (const auto &m : handler.error_msgs)
      if (m.find(needle) != std::string::npos) return true;
    return false;
  }
};

constexpr int kXor = 1;
constexpr int kAes256 = 16;

}  // namespace

TEST(NscaServerProtocol, OnReadDeliversADecryptedResult) {
  for (const int method : {0, kXor, kAes256}) {
    if (!nscp::encryption::engine::hasEncryption(method)) continue;
    Exchange x(method);
    // A complete packet ends the exchange: on_read says "no more reads".
    EXPECT_FALSE(x.feed(client_packet(x.proto->get_outbound(), "secret", method, 0))) << method;
    ASSERT_EQ(x.handler.received_packets.size(), 1u) << method;
    const nsca::packet &p = x.handler.received_packets.front();
    EXPECT_EQ(p.host, "web01");
    EXPECT_EQ(p.service, "disk");
    EXPECT_EQ(p.result, "CRITICAL: C: 99% used");
    EXPECT_EQ(p.code, 2u);
    EXPECT_TRUE(x.handler.error_msgs.empty()) << method;
  }
}

// TCP may deliver the packet in pieces: the protocol keeps reading until it
// has a whole one.
TEST(NscaServerProtocol, OnReadAssemblesAPacketFromChunks) {
  Exchange x(kXor);
  const std::string data = client_packet(x.proto->get_outbound(), "secret", kXor, 0);
  EXPECT_TRUE(x.feed(data.substr(0, 10)));
  EXPECT_TRUE(x.feed(data.substr(10, 200)));
  EXPECT_TRUE(x.handler.received_packets.empty());
  EXPECT_FALSE(x.feed(data.substr(210)));
  EXPECT_EQ(x.handler.received_packets.size(), 1u);
}

// The replay window: a packet more than five minutes off the server clock,
// either way, is dropped even though it decrypts and its CRC is right.
TEST(NscaServerProtocol, OnReadRejectsTimestampsOutsideTheReplayWindow) {
  for (const int skew : {-6 * 60, 6 * 60, -24 * 60 * 60}) {
    Exchange x(kXor);
    EXPECT_FALSE(x.feed(client_packet(x.proto->get_outbound(), "secret", kXor, skew))) << skew;
    EXPECT_TRUE(x.handler.received_packets.empty()) << skew;
    EXPECT_TRUE(x.any_error_contains("timestamp out of range")) << skew;
  }
}

TEST(NscaServerProtocol, OnReadAcceptsTimestampsInsideTheReplayWindow) {
  for (const int skew : {-4 * 60, 4 * 60}) {
    Exchange x(kXor);
    x.feed(client_packet(x.proto->get_outbound(), "secret", kXor, skew));
    EXPECT_EQ(x.handler.received_packets.size(), 1u) << skew;
  }
}

// A client with the wrong password produces garbage after decryption; the
// CRC (or the version) catches it and nothing is handled.
TEST(NscaServerProtocol, OnReadRejectsTheWrongPassword) {
  for (const int method : {kXor, kAes256}) {
    if (!nscp::encryption::engine::hasEncryption(method)) continue;
    Exchange x(method, "secret");
    EXPECT_FALSE(x.feed(client_packet(x.proto->get_outbound(), "not-the-secret", method, 0)));
    EXPECT_TRUE(x.handler.received_packets.empty()) << method;
    EXPECT_TRUE(x.any_error_contains("Exception processing request")) << method;
  }
}

// A client using a different IV - a replayed packet from an earlier
// connection - does not decrypt under this connection's IV.
TEST(NscaServerProtocol, OnReadRejectsAPacketEncryptedForAnotherConnection) {
  if (!nscp::encryption::engine::hasEncryption(kAes256)) GTEST_SKIP() << "built without crypto++";
  Exchange earlier(kAes256);
  Exchange now(kAes256);
  EXPECT_FALSE(now.feed(client_packet(earlier.proto->get_outbound(), "secret", kAes256, 0)));
  EXPECT_TRUE(now.handler.received_packets.empty());
}
