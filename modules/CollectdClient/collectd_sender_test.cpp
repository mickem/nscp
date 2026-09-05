// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the UDP delivery half of CollectdClient: what
// collectd::send_datagrams() puts on the wire, and what it reports back when
// it cannot. The datagrams are read back from a real loopback UDP socket
// rather than a mock, so the test covers the socket setup (address family,
// resolution) and not only the bookkeeping - the same approach as
// SyslogClient's sink test.

#include <net/collectd/collectd_sender.hpp>

#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include <algorithm>

#include <chrono>
#include <list>
#include <string>
#include <vector>

namespace {
using boost::asio::ip::udp;

// A UDP socket bound to an ephemeral loopback port, and the datagrams it saw.
class udp_sink {
 public:
  udp_sink() : socket_(io_, udp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0)) {}

  unsigned short port() const { return socket_.local_endpoint().port(); }
  std::string port_string() const { return std::to_string(port()); }

  // Drain whatever has arrived. Non-blocking: everything under test sends
  // before this is called, and a datagram that never arrives must fail the
  // test rather than hang it.
  std::vector<std::string> drain() {
    std::vector<std::string> out;
    socket_.non_blocking(true);
    for (;;) {
      char buffer[2048];
      boost::system::error_code ec;
      const std::size_t len = socket_.receive(boost::asio::buffer(buffer), 0, ec);
      if (ec) break;
      out.push_back(std::string(buffer, len));
    }
    return out;
  }

 private:
  boost::asio::io_context io_;
  udp::socket socket_;
};
}  // namespace

TEST(CollectdSender, SendsEveryDatagramToAnIpLiteralTarget) {
  udp_sink sink;
  const std::list<std::string> datagrams = {"one", "two", "three"};

  const collectd::sender_result result = collectd::send_datagrams(collectd::sender_config("127.0.0.1", sink.port_string()), datagrams);

  EXPECT_EQ(result.sent, 3u);
  EXPECT_EQ(result.failed, 0u);
  // One send call per datagram: a successful send never spends retry budget.
  EXPECT_EQ(result.attempts, 3u);
  EXPECT_TRUE(result.errors.empty());
  const std::vector<std::string> received = sink.drain();
  ASSERT_EQ(received.size(), 3u);
  EXPECT_EQ(received[0], "one");
  EXPECT_EQ(received[2], "three");
}

TEST(CollectdSender, ResolvesAHostNameTarget) {
  // The send path used to parse IP literals only, so a target configured with
  // a host name threw on every metrics cycle and nothing was ever sent.
  udp_sink sink;

  const collectd::sender_result result = collectd::send_datagrams(collectd::sender_config("localhost", sink.port_string()), {"payload"});

  EXPECT_TRUE(result.errors.empty());
  EXPECT_EQ(result.sent, 1u);
  const std::vector<std::string> received = sink.drain();
  ASSERT_EQ(received.size(), 1u);
  EXPECT_EQ(received[0], "payload");
}

TEST(CollectdSender, ReportsAnUnresolvableTarget) {
  const collectd::sender_result result =
      collectd::send_datagrams(collectd::sender_config("no-such-host.invalid", "25826"), {"a", "b"});

  EXPECT_EQ(result.sent, 0u);
  EXPECT_EQ(result.failed, 2u);
  ASSERT_EQ(result.errors.size(), 1u);
  // The message must name the target: a bare resolver error is unactionable.
  EXPECT_NE(result.errors.front().find("no-such-host.invalid"), std::string::npos);
}

TEST(CollectdSender, SkipsEmptyDatagrams) {
  udp_sink sink;

  const collectd::sender_result result = collectd::send_datagrams(collectd::sender_config("127.0.0.1", sink.port_string()), {"", "kept", ""});

  EXPECT_EQ(result.sent, 1u);
  const std::vector<std::string> received = sink.drain();
  ASSERT_EQ(received.size(), 1u);
  EXPECT_EQ(received[0], "kept");
}

TEST(CollectdSender, NothingToSendIsNotAnError) {
  const collectd::sender_result result = collectd::send_datagrams(collectd::sender_config("127.0.0.1", "25826"), {});

  EXPECT_EQ(result.sent, 0u);
  EXPECT_EQ(result.failed, 0u);
  EXPECT_TRUE(result.errors.empty());
}

// A datagram larger than the 65507-byte UDP maximum always fails to send, which
// makes the failure path deterministic without an unreachable network.
namespace {
std::string oversized_datagram() { return std::string(70000, 'x'); }
}  // namespace

TEST(CollectdSender, RetriesAFailingSend) {
  udp_sink sink;
  const int retries = 2;

  const collectd::sender_result result =
      collectd::send_datagrams(collectd::sender_config("127.0.0.1", sink.port_string(), retries), {oversized_datagram()});

  EXPECT_EQ(result.sent, 0u);
  EXPECT_EQ(result.failed, 1u);
  // The configured retries are actually spent: one initial attempt plus two.
  EXPECT_EQ(result.attempts, static_cast<std::size_t>(retries + 1));
  ASSERT_EQ(result.errors.size(), 1u);
  EXPECT_NE(result.errors.front().find(sink.port_string()), std::string::npos);
}

TEST(CollectdSender, ReportsEachDistinctFailureOnce) {
  udp_sink sink;

  const collectd::sender_result result = collectd::send_datagrams(collectd::sender_config("127.0.0.1", sink.port_string()),
                                                                  {oversized_datagram(), oversized_datagram(), oversized_datagram()});

  EXPECT_EQ(result.failed, 3u);
  // Three identical failures, one log line: a broken target must not flood the
  // log every metrics interval.
  EXPECT_EQ(result.errors.size(), 1u);
}

TEST(CollectdSender, StopsRetryingWhenTheTimeoutExpires) {
  udp_sink sink;
  // A retry budget large enough to run for minutes, bounded by a one second
  // timeout: the metrics thread must come back on time regardless.
  const collectd::sender_config config("127.0.0.1", sink.port_string(), 100000, 1);

  const auto started = std::chrono::steady_clock::now();
  const collectd::sender_result result = collectd::send_datagrams(config, {oversized_datagram()});
  const auto elapsed = std::chrono::steady_clock::now() - started;

  EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(), 10);
  EXPECT_GT(result.attempts, 1u);
  EXPECT_LT(result.attempts, 100000u);
  EXPECT_EQ(result.failed, 1u);
}

TEST(CollectdSender, AbandonsRemainingDatagramsWhenTheTimeoutExpires) {
  udp_sink sink;
  // The first datagram burns the whole budget; the rest must be reported as
  // not sent rather than attempted anyway.
  const collectd::sender_config config("127.0.0.1", sink.port_string(), 100000, 1);

  const collectd::sender_result result = collectd::send_datagrams(config, {oversized_datagram(), "second", "third"});

  EXPECT_EQ(result.sent, 0u);
  EXPECT_EQ(result.failed, 3u);
  EXPECT_EQ(sink.drain().size(), 0u);
  const bool timed_out = std::any_of(result.errors.begin(), result.errors.end(),
                                     [](const std::string &e) { return e.find("Timed out") != std::string::npos; });
  EXPECT_TRUE(timed_out);
}
