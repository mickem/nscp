// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// One outbound connection to an Icinga 2 parent (master or satellite).
//
// Everything runs on the io_context the connection was created on, which is
// driven by a single thread: reads, the write queue and the heartbeat timer
// never race. Check execution happens elsewhere and comes back through
// send(), which posts onto that io_context.

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/json.hpp>
#include <deque>
#include <functional>
#include <memory>
#include <string>

#include "icinga_agent_protocol.hpp"

namespace icinga_agent {

struct connection_config {
  std::string host;
  std::string port = "5665";
  std::string parent_endpoint;  // expected CN of the parent; empty = do not check
  std::string ticket;
  std::string agent_version;
  int heartbeat_interval = 20;  // seconds
  int idle_timeout = 120;       // seconds without anything from the parent
};

class connection : public std::enable_shared_from_this<connection> {
 public:
  struct callbacks {
    std::function<void(const std::string &)> log_info;
    std::function<void(const std::string &)> log_error;
    std::function<void(const std::string &)> log_debug;
    // An event::ExecuteCommand. The handler runs it and calls send() with the
    // event::CheckResult.
    std::function<void(std::shared_ptr<connection>, const boost::json::value &params)> execute;
    std::function<void(const boost::json::value &params)> update_certificate;
    std::function<void()> closed;
  };

  connection(boost::asio::io_context &io, boost::asio::ssl::context &ctx, connection_config config, callbacks cb);

  void start();
  void close(const std::string &reason);
  // Thread-safe: queue a framed message.
  void send(std::string framed);

 private:
  void on_resolve(const boost::system::error_code &ec, const boost::asio::ip::tcp::resolver::results_type &results);
  void on_connect(const boost::system::error_code &ec);
  void on_handshake(const boost::system::error_code &ec);
  void read();
  void on_read(const boost::system::error_code &ec, std::size_t bytes);
  void dispatch(const std::string &payload);
  void write_next();
  void schedule_heartbeat();
  void arm_idle_timer();

  boost::asio::io_context &io_;
  boost::asio::ip::tcp::resolver resolver_;
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream_;
  boost::asio::steady_timer heartbeat_timer_;
  boost::asio::steady_timer idle_timer_;
  connection_config config_;
  callbacks cb_;
  netstring_reader reader_;
  std::array<char, 16 * 1024> buffer_{};
  std::deque<std::string> outbox_;
  bool writing_ = false;
  bool closed_ = false;
  bool established_ = false;
};

}  // namespace icinga_agent
