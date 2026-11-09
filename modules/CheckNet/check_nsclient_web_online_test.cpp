// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_nsclient_web_online against a listener on 127.0.0.1: the timeout
// bounds a peer that accepts and never answers, and is validated.

#include "check_nsclient_web_online.h"

#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>
#include <thread>
#include <vector>

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

using boost::asio::ip::tcp;

// Accepts one connection and holds it, saying nothing, until the peer leaves.
class silent_server {
 public:
  silent_server() : acceptor_(io_, tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0)) {
    port_ = acceptor_.local_endpoint().port();
    thread_ = std::thread([this] {
      tcp::socket s(io_);
      boost::system::error_code ec;
      acceptor_.accept(s, ec);
      char buf[1024];
      while (!ec && !stopping_) s.read_some(boost::asio::buffer(buf), ec);
    });
  }
  ~silent_server() {
    stopping_ = true;
    // Unblock an accept that never happened, then wait for the reader.
    boost::asio::io_context io;
    tcp::socket poke(io);
    boost::system::error_code ec;
    poke.connect(tcp::endpoint(boost::asio::ip::address_v4::loopback(), port_), ec);
    poke.close(ec);
    thread_.join();
  }
  std::string port() const { return std::to_string(port_); }

 private:
  boost::asio::io_context io_;
  tcp::acceptor acceptor_;
  unsigned short port_ = 0;
  std::atomic<bool> stopping_{false};
  std::thread thread_;
};

PB::Commands::QueryResponseMessage::Response run(const std::vector<std::string> &args) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_nsclient_web_online");
  for (const std::string &a : args) request.add_arguments(a);
  PB::Commands::QueryResponseMessage::Response response;
  check_net::check_nsclient_web_online("none", request, &response);
  return response;
}

std::string message(const PB::Commands::QueryResponseMessage::Response &r) { return r.lines_size() > 0 ? r.lines(0).message() : ""; }

}  // namespace

TEST(CheckNsclientWebOnlineTimeout, SilentAgentTimesOutOverHttp) {
  silent_server server;
  const auto start = std::chrono::steady_clock::now();
  const auto r = run({"url=http://127.0.0.1:" + server.port(), "timeout=1000"});
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(10));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.result()) << message(r);
  EXPECT_NE(std::string::npos, message(r).find("Failed to reach http://127.0.0.1:" + server.port())) << message(r);
}

TEST(CheckNsclientWebOnlineTimeout, SilentAgentTimesOutInTheTlsHandshake) {
  // host= defaults to https: the client sends a ClientHello and waits for a
  // ServerHello that never comes. This is where the check used to hang.
  silent_server server;
  const auto start = std::chrono::steady_clock::now();
  const auto r = run({"--host", "127.0.0.1", "--port", server.port(), "--timeout", "1000", "verify=none"});
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(10));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.result()) << message(r);
  EXPECT_NE(std::string::npos, message(r).find("Failed to reach https://127.0.0.1:" + server.port())) << message(r);
}

TEST(CheckNsclientWebOnlineTimeout, InvalidTimeoutsAreRejected) {
  for (const std::string &bad : {"0", "-5", "soon", "10s", "86400001"}) {
    const auto r = run({"url=http://127.0.0.1:1", "timeout=" + bad});
    EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.result()) << bad;
    EXPECT_EQ("Invalid timeout: " + bad + " (expected 1-86400000 milliseconds)", message(r)) << bad;
  }
}
