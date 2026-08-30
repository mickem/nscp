// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for the Graphite client's carbon line rendering, in particular the
// sanitization that keeps attacker-influenced text (check aliases arrive from
// remote submitters via NSCA/forwarded results, perfdata labels from checks)
// from injecting extra metric lines or carbon tags into the plaintext
// "<path> <value> <ts>\n" protocol.
//
// The submission itself is captured by a loopback TCP server standing in for
// carbon, so what is asserted is the exact byte stream a carbon receiver
// would parse.

#include "graphite_client.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <boost/asio.hpp>
#include <chrono>
#include <client/command_line_parser.hpp>
#include <map>
#include <str/xtos.hpp>
#include <string>
#include <thread>
#include <vector>

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

using boost::asio::ip::tcp;

client::destination_container container_with(const std::map<std::string, std::string> &options) {
  client::destination_container d;
  for (const auto &o : options) d.set_string_data(o.first, o.second);
  return d;
}

// Accepts one connection and keeps every byte received until the peer closes,
// which matches how GraphiteClient submits: connect, write all lines, close.
//
// Everything runs async on one io_context with a hard deadline, so a
// submission that fails before connecting (or stalls mid-stream) cannot leave
// the server thread blocked in accept()/read() and wedge the join() in
// received() / the destructor - the deadline cancels the pending operation,
// run() returns, and the assertions fail loudly on whatever was captured.
class loopback_carbon_receiver {
 public:
  explicit loopback_carbon_receiver(const std::chrono::seconds deadline = std::chrono::seconds(30))
      : acceptor_(io_, {tcp::v4(), 0}), socket_(io_), deadline_(io_) {
    port_ = acceptor_.local_endpoint().port();
    deadline_.expires_after(deadline);
    deadline_.async_wait([this](const boost::system::error_code &ec) {
      if (!ec) stop();
    });
    acceptor_.async_accept(socket_, [this](const boost::system::error_code &ec) {
      if (ec)
        stop();
      else
        start_read();
    });
    thread_ = std::thread([this] { io_.run(); });
  }
  ~loopback_carbon_receiver() {
    if (thread_.joinable()) thread_.join();
  }
  unsigned short port() const { return port_; }
  std::string received() {
    if (thread_.joinable()) thread_.join();
    return received_;
  }

 private:
  void start_read() {
    socket_.async_read_some(boost::asio::buffer(buf_), [this](const boost::system::error_code &ec, const std::size_t n) {
      if (n > 0) received_.append(buf_, n);
      if (ec)
        stop();  // EOF: the client wrote its lines and closed.
      else
        start_read();
    });
  }
  void stop() {
    boost::system::error_code ignored;
    acceptor_.close(ignored);
    socket_.close(ignored);
    try {
      deadline_.cancel();
    } catch (...) {
    }
  }

  boost::asio::io_context io_;
  tcp::acceptor acceptor_;
  tcp::socket socket_;
  boost::asio::steady_timer deadline_;
  std::thread thread_;
  unsigned short port_ = 0;
  char buf_[4096] = {};
  std::string received_;
};

std::vector<std::string> split_lines(const std::string &s) {
  std::vector<std::string> lines;
  std::string::size_type start = 0;
  for (;;) {
    const std::string::size_type pos = s.find('\n', start);
    if (pos == std::string::npos) {
      if (start < s.size()) lines.push_back(s.substr(start));
      return lines;
    }
    lines.push_back(s.substr(start, pos - start));
    start = pos + 1;
  }
}

}  // namespace

// Regression test for the review finding that the receiver could wedge the
// test process: when no client ever connects (a submission failing before the
// connect), received() must return once the deadline fires instead of
// blocking forever in join().
TEST(GraphiteLoopbackReceiverTest, does_not_hang_when_no_client_connects) {
  const auto start = std::chrono::steady_clock::now();
  loopback_carbon_receiver server{std::chrono::seconds(1)};
  EXPECT_EQ("", server.received());
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(10));
}

TEST(GraphiteFixStringTest, replaces_line_and_field_separators) {
  EXPECT_EQ("a_b", graphite_client::fix_graphite_string("a b"));
  EXPECT_EQ("a_b", graphite_client::fix_graphite_string("a\nb"));
  EXPECT_EQ("a_b", graphite_client::fix_graphite_string("a\rb"));
  EXPECT_EQ("a_b", graphite_client::fix_graphite_string("a\tb"));
  EXPECT_EQ("a_b", graphite_client::fix_graphite_string("a;b"));
  EXPECT_EQ("a_b", graphite_client::fix_graphite_string(std::string("a\0b", 3)));
  EXPECT_EQ("a_b", graphite_client::fix_graphite_string("a\\b"));
  EXPECT_EQ("_a__b_", graphite_client::fix_graphite_string("[a](b)"));
  EXPECT_EQ("apercent", graphite_client::fix_graphite_string("a%"));
}

TEST(GraphiteFixStringTest, leaves_normal_metric_text_alone) {
  EXPECT_EQ("nsclient.host-1.cpu.total", graphite_client::fix_graphite_string("nsclient.host-1.cpu.total"));
  EXPECT_EQ("42.5", graphite_client::fix_graphite_string("42.5"));
}

TEST(GraphiteMakeLineTest, scrubs_value_so_it_cannot_split_the_line) {
  graphite_client::g_data d;
  d.path = "nsclient.host.check.metric";
  d.value = "1 2;3\n4";
  EXPECT_EQ("nsclient.host.check.metric 1_2_3_4 1234\n", graphite_client::graphite_client_handler::make_line(d, "1234"));
}

// A hostile alias must not be able to inject extra carbon lines through the
// status path (which historically was only scrubbed for spaces, unlike the
// perf path) nor through the perf path.
TEST(GraphiteSubmitTest, hostile_alias_and_perf_label_cannot_inject_metric_lines) {
  loopback_carbon_receiver server;

  client::destination_container sender = container_with({{"host", "sender-host"}});
  client::destination_container target = container_with({
      {"address", "127.0.0.1:" + str::xtos(server.port())},
      {"perf path", "nsclient.${hostname}.${check_alias}.${perf_alias}"},
      {"status path", "nsclient.${hostname}.${check_alias}.status"},
      {"send perfdata", "true"},
      {"send status", "true"},
  });

  PB::Commands::SubmitRequestMessage request;
  auto *payload = request.add_payload();
  payload->set_command("check_evil");
  payload->set_alias("evil\ninjected.metric 666");
  payload->set_result(PB::Common::ResultCode::OK);
  auto *line = payload->add_lines();
  line->set_message("all good");
  auto *perf = line->add_perf();
  perf->set_alias("rate;tag=x");
  perf->mutable_float_value()->set_value(42.5);

  PB::Commands::SubmitResponseMessage response;
  graphite_client::graphite_client_handler handler;
  ASSERT_TRUE(handler.submit(sender, target, request, response));
  ASSERT_EQ(1, response.payload_size());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, response.payload(0).result().code()) << response.payload(0).result().message();

  const std::string data = server.received();
  const std::vector<std::string> lines = split_lines(data);
  // One perf line + one status line. If the newline in the alias survived,
  // there would be a third, attacker-chosen line starting "injected.metric".
  ASSERT_EQ(2u, lines.size()) << "raw capture: " << data;
  EXPECT_EQ("nsclient.sender-host.evil_injected.metric_666.rate_tag=x 42.5", lines[0].substr(0, lines[0].rfind(' ')));
  EXPECT_EQ("nsclient.sender-host.evil_injected.metric_666.status 0", lines[1].substr(0, lines[1].rfind(' ')));
  for (const std::string &l : lines) {
    EXPECT_EQ(std::string::npos, l.find(';')) << l;
    // Exactly "<path> <value> <ts>": two spaces, no more.
    EXPECT_EQ(2, std::count(l.begin(), l.end(), ' ')) << l;
  }
}

TEST(GraphiteSubmitTest, benign_submission_renders_expected_lines) {
  loopback_carbon_receiver server;

  client::destination_container sender = container_with({{"host", "myhost"}});
  client::destination_container target = container_with({
      {"address", "127.0.0.1:" + str::xtos(server.port())},
      {"perf path", "nsclient.${hostname}.${check_alias}.${perf_alias}"},
      {"status path", "nsclient.${hostname}.${check_alias}.status"},
      {"send perfdata", "true"},
      {"send status", "true"},
  });

  PB::Commands::SubmitRequestMessage request;
  auto *payload = request.add_payload();
  payload->set_command("check_cpu");
  payload->set_alias("cpu load");
  payload->set_result(PB::Common::ResultCode::WARNING);
  auto *line = payload->add_lines();
  line->set_message("warning");
  auto *perf = line->add_perf();
  perf->set_alias("total 5m");
  perf->mutable_float_value()->set_value(87);

  PB::Commands::SubmitResponseMessage response;
  graphite_client::graphite_client_handler handler;
  ASSERT_TRUE(handler.submit(sender, target, request, response));

  const std::vector<std::string> lines = split_lines(server.received());
  ASSERT_EQ(2u, lines.size());
  EXPECT_EQ(0u, lines[0].find("nsclient.myhost.cpu_load.total_5m 87 ")) << lines[0];
  EXPECT_EQ(0u, lines[1].find("nsclient.myhost.cpu_load.status 1 ")) << lines[1];
}
