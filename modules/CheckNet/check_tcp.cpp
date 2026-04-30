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

#include "check_tcp.h"

#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>

#include "check_net_error.hpp"

namespace po = boost::program_options;

namespace check_net {
namespace check_tcp_filter {

filter_obj_handler::filter_obj_handler() {
  registry_.add_string("host", &filter_obj::get_host, "Host the check connected to");
  registry_.add_string("result", &filter_obj::get_result, "Textual result of the check (ok, refused, timeout, no_match, ...)");
  registry_.add_int_x("port", parsers::where::type_int, &filter_obj::get_port, "TCP port the check connected to");
  registry_.add_int_x("time", parsers::where::type_int, &filter_obj::get_time, "Connection time in milliseconds");
  registry_.add_int_x("connected", parsers::where::type_int, &filter_obj::get_connected, "1 when the connection succeeded, 0 otherwise");
}

}  // namespace check_tcp_filter

namespace {

// Synchronous TCP connect with millisecond timeout.
// Optionally writes "send_data" and reads the response, looking for "expect" as a substring.
// "result" gets a short status word (connected/timeout/refused/no_match/error).
void run_tcp_check(const std::string &host, unsigned short port, int timeout_ms, const std::string &send_data, const std::string &expect,
                   check_tcp_filter::filter_obj &out) {
  using boost::asio::ip::tcp;

  out.host = host;
  out.port = port;
  out.connected = false;
  out.result = "error";
  out.time = 0;

  boost::asio::io_service io_service;
  tcp::resolver resolver(io_service);
  tcp::socket socket(io_service);
  boost::asio::deadline_timer timer(io_service);

  const auto start = boost::chrono::steady_clock::now();
  bool connect_done = false;
  boost::system::error_code connect_ec = boost::asio::error::would_block;

  try {
    tcp::resolver::query query(host, std::to_string(port));
    boost::system::error_code resolve_ec;
    auto endpoints = resolver.resolve(query, resolve_ec);
    if (resolve_ec) {
      out.result = "resolve_failed";
      return;
    }

    timer.expires_from_now(boost::posix_time::milliseconds(timeout_ms));
    timer.async_wait([&](const boost::system::error_code &ec) {
      if (!ec && !connect_done) {
        boost::system::error_code ignore;
        socket.close(ignore);
      }
    });

    boost::asio::async_connect(socket, endpoints, [&](const boost::system::error_code &ec, const tcp::endpoint &) {
      connect_ec = ec;
      connect_done = true;
      boost::system::error_code ignore;
      timer.cancel(ignore);
    });

    io_service.run();
    io_service.reset();

    const auto elapsed = boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::steady_clock::now() - start).count();
    out.time = static_cast<long long>(elapsed);

    if (connect_ec) {
      if (connect_ec == boost::asio::error::connection_refused) {
        out.result = "refused";
      } else if (connect_ec == boost::asio::error::operation_aborted) {
        out.result = "timeout";
      } else {
        out.result = "error";
      }
      return;
    }

    out.connected = true;
    out.result = "ok";

    if (!send_data.empty()) {
      boost::system::error_code write_ec;
      boost::asio::write(socket, boost::asio::buffer(send_data), write_ec);
      if (write_ec) {
        out.result = "write_failed";
        return;
      }
    }

    if (!expect.empty()) {
      // Read whatever the peer sends within a small window, with a deadline.
      boost::asio::streambuf response_buf;
      boost::system::error_code read_ec = boost::asio::error::would_block;
      bool read_done = false;

      timer.expires_from_now(boost::posix_time::milliseconds(timeout_ms));
      timer.async_wait([&](const boost::system::error_code &ec) {
        if (!ec && !read_done) {
          boost::system::error_code ignore;
          socket.close(ignore);
        }
      });

      boost::asio::async_read(socket, response_buf, boost::asio::transfer_at_least(1), [&](const boost::system::error_code &ec, std::size_t) {
        read_ec = ec;
        read_done = true;
        boost::system::error_code ignore;
        timer.cancel(ignore);
      });

      io_service.run();

      if (read_ec && read_ec != boost::asio::error::eof) {
        out.result = "read_failed";
        return;
      }

      const std::string data{boost::asio::buffers_begin(response_buf.data()), boost::asio::buffers_end(response_buf.data())};
      if (data.find(expect) == std::string::npos) {
        out.result = "no_match";
      } else {
        out.result = "ok";
      }
    }
  } catch (const std::exception &e) {
    out.result = std::string("error: ") + check_net::format_exception_message(e);
  }

  boost::system::error_code ignore;
  socket.close(ignore);
}

}  // namespace

void check_tcp(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using check_tcp_filter::filter;
  using check_tcp_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  std::vector<std::string> hosts;
  std::string hosts_string;
  unsigned short port = 0;
  int timeout_ms = 5000;
  std::string send_data;
  std::string expect;

  filter f;
  filter_helper.add_options("time > 1000", "time > 5000 or result != 'ok'", "", f.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${problem_list}", "${host}:${port} ${result} in ${time}ms", "${host}_${port}", "No hosts checked", "%(status): %(list)");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("host", po::value<std::vector<std::string> >(&hosts), "Host(s) to connect to (may be given multiple times).")
    ("hosts", po::value<std::string>(&hosts_string), "Comma separated list of hosts to connect to.")
    ("port", po::value<unsigned short>(&port), "TCP port to connect to.")
    ("timeout", po::value<int>(&timeout_ms)->default_value(5000), "Connection / read timeout in milliseconds.")
    ("send", po::value<std::string>(&send_data), "Optional payload to send after the connection is established.")
    ("expect", po::value<std::string>(&expect), "Optional substring expected in the response (requires --send or returns whatever the peer sent first).")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (!hosts_string.empty()) {
    std::vector<std::string> tmp;
    boost::split(tmp, hosts_string, boost::is_any_of(","));
    for (auto &h : tmp) {
      boost::trim(h);
      if (!h.empty()) hosts.push_back(h);
    }
  }

  if (hosts.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No host specified");
  if (port == 0) return nscapi::protobuf::functions::set_response_bad(*response, "No port specified");

  if (!filter_helper.build_filter(f)) return;

  for (const auto &host : hosts) {
    auto obj = boost::make_shared<filter_obj>();
    run_tcp_check(host, port, timeout_ms, send_data, expect, *obj);
    f.match(obj);
  }

  filter_helper.post_process(f);
}

}  // namespace check_net
