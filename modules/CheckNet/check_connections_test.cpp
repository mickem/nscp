// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// End-to-end tests for check_connections on Linux, where it reads the live
// /proc/net tables. The test opens its own listener and connection first, so
// there is always at least one LISTEN and one ESTABLISHED socket to count no
// matter what else the host is doing; assertions are lower bounds only.

#include "check_connections.h"

#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <cstdio>
#include <string>
#include <vector>

#if defined(__linux__)

namespace {

using boost::asio::ip::tcp;

struct outcome {
  PB::Common::ResultCode code;
  std::string message;
  std::string perf;
};

outcome run(const std::vector<std::string> &args) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_connections");
  for (const std::string &a : args) request.add_arguments(a);
  PB::Commands::QueryResponseMessage::Response response;
  check_net::check_connections(request, &response);
  outcome r;
  r.code = response.result();
  for (int i = 0; i < response.lines_size(); ++i) {
    if (!r.message.empty()) r.message += "\n";
    r.message += response.lines(i).message();
    for (int j = 0; j < response.lines(i).perf_size(); ++j) r.perf += response.lines(i).perf(j).alias() + ";";
  }
  return r;
}

// A listener plus one connection to it: one LISTEN socket and two ESTABLISHED
// ends on 127.0.0.1.
class CheckConnectionsLive : public ::testing::Test {
 protected:
  boost::asio::io_context io_;
  tcp::acceptor acceptor_{io_, tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0)};
  tcp::socket client_{io_};
  tcp::socket server_{io_};

  void SetUp() override {
    client_.connect(acceptor_.local_endpoint());
    acceptor_.accept(server_);
  }
};

}  // namespace

TEST_F(CheckConnectionsLive, TotalBucketCountsOurSockets) {
  const outcome r = run({"detail-syntax=${listen} ${established} ${total_connections}", "show-all"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  long long listen = 0, established = 0, total = 0;
  const std::string::size_type at = r.message.find(": ");
  ASSERT_NE(std::string::npos, at) << r.message;
  ASSERT_EQ(3, std::sscanf(r.message.c_str() + at + 2, "%lld %lld %lld", &listen, &established, &total)) << r.message;
  EXPECT_GE(listen, 1);
  EXPECT_GE(established, 2);
  EXPECT_GE(total, listen + established);
}

TEST_F(CheckConnectionsLive, PerStateBucketsAreReported) {
  const outcome r = run({"filter=protocol = 'tcp' and family = 'any'", "detail-syntax=${state}=${connections}", "show-all"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("LISTEN=")) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("ESTABLISHED=")) << r.message;
}

TEST_F(CheckConnectionsLive, PerProtocolBucketsAreReported) {
  const outcome r = run({"filter=state = 'all' and protocol != 'total'", "detail-syntax=${protocol}/${family}", "show-all"});
  EXPECT_NE(std::string::npos, r.message.find("tcp/ipv4")) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("udp6/ipv6")) << r.message;
}

TEST_F(CheckConnectionsLive, ThresholdsApplyToTheTotal) {
  const outcome r = run({"critical=listen > 0", "detail-syntax=${listen}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
}

TEST_F(CheckConnectionsLive, DefaultPerfdataCoversTheStates) {
  const outcome r = run({});
  EXPECT_NE(std::string::npos, r.perf.find("listen")) << r.perf;
  EXPECT_NE(std::string::npos, r.perf.find("established")) << r.perf;
}

#endif  // __linux__
