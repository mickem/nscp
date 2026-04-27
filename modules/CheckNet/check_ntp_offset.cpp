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

#include "check_ntp_offset.h"

#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>

#include <chrono>
#include <cstdint>

namespace po = boost::program_options;

namespace check_net {
namespace check_ntp_filter {

filter_obj_handler::filter_obj_handler() {
  registry_.add_string("server", &filter_obj::get_server, "NTP server that was queried");
  registry_.add_string("result", &filter_obj::get_result, "Textual result of the query (ok, timeout, error, ...)");
  registry_.add_int_x("port", parsers::where::type_int, &filter_obj::get_port, "UDP port the query was sent to");
  registry_.add_int_x("offset", parsers::where::type_int, &filter_obj::get_offset, "Absolute clock offset between local host and server, in milliseconds");
  registry_.add_int_x("offset_signed", parsers::where::type_int, &filter_obj::get_offset_signed,
                      "Signed clock offset (positive = local clock is ahead of server), in milliseconds");
  registry_.add_int_x("stratum", parsers::where::type_int, &filter_obj::get_stratum, "Stratum reported by the server (0..16)");
  registry_.add_int_x("time", parsers::where::type_int, &filter_obj::get_time, "Round trip time of the NTP query in milliseconds");
}

}  // namespace check_ntp_filter

namespace {

// NTP epoch (1900-01-01) is 2208988800 seconds before the unix epoch.
constexpr std::uint64_t kNtpUnixDelta = 2208988800ULL;

// Convert an NTP timestamp (uint32 seconds, uint32 fraction) read from the wire
// to milliseconds since the unix epoch.
long long ntp_to_unix_ms(std::uint32_t secs, std::uint32_t frac) {
  if (secs == 0 && frac == 0) return 0;
  const std::uint64_t s = static_cast<std::uint64_t>(secs) - kNtpUnixDelta;
  // Fractional part is in units of 2^-32 seconds.
  const long long frac_ms = static_cast<long long>((static_cast<std::uint64_t>(frac) * 1000ULL) >> 32);
  return static_cast<long long>(s) * 1000LL + frac_ms;
}

void run_ntp_check(const std::string &server, unsigned short port, int timeout_ms, check_ntp_filter::filter_obj &out) {
  using boost::asio::ip::udp;

  out.server = server;
  out.port = port;
  out.result = "error";
  out.offset_ms = 0;
  out.stratum = 0;
  out.time = 0;

  boost::asio::io_service io_service;
  udp::resolver resolver(io_service);
  udp::socket socket(io_service);
  boost::asio::deadline_timer timer(io_service);

  try {
    udp::resolver::query query(udp::v4(), server, std::to_string(port));
    boost::system::error_code resolve_ec;
    auto it = resolver.resolve(query, resolve_ec);
    if (resolve_ec || it == udp::resolver::iterator()) {
      out.result = "resolve_failed";
      return;
    }
    udp::endpoint endpoint = *it;

    socket.open(udp::v4());

    // Build the NTP request packet (NTPv3 client mode, widely accepted by NTPv4 servers).
    // Byte 0 layout: LI (2 bits) | VN (3 bits) | Mode (3 bits)
    // 0x1b = 0b00011011 -> LI=0, VN=3, Mode=3 (client).
    unsigned char req[48] = {0};
    req[0] = 0x1b;

    // Local send timestamp (T1) used as offset reference.
    const auto t1_steady = boost::chrono::steady_clock::now();
    const auto t1_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    boost::system::error_code send_ec;
    socket.send_to(boost::asio::buffer(req, sizeof(req)), endpoint, 0, send_ec);
    if (send_ec) {
      out.result = "send_failed";
      return;
    }

    unsigned char resp_buf[48] = {0};
    udp::endpoint sender;

    boost::system::error_code recv_ec = boost::asio::error::would_block;
    std::size_t bytes_received = 0;
    bool recv_done = false;

    timer.expires_from_now(boost::posix_time::milliseconds(timeout_ms));
    timer.async_wait([&](const boost::system::error_code &ec) {
      if (!ec && !recv_done) {
        boost::system::error_code ignore;
        socket.close(ignore);
      }
    });

    socket.async_receive_from(boost::asio::buffer(resp_buf, sizeof(resp_buf)), sender,
                              [&](const boost::system::error_code &ec, std::size_t n) {
                                recv_ec = ec;
                                bytes_received = n;
                                recv_done = true;
                                boost::system::error_code ignore;
                                timer.cancel(ignore);
                              });

    io_service.run();

    const auto t4_steady = boost::chrono::steady_clock::now();
    const auto rtt_ms = boost::chrono::duration_cast<boost::chrono::milliseconds>(t4_steady - t1_steady).count();
    const auto t4_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    out.time = static_cast<long long>(rtt_ms);

    if (recv_ec) {
      if (recv_ec == boost::asio::error::operation_aborted) {
        out.result = "timeout";
      } else {
        out.result = "recv_failed";
      }
      return;
    }
    if (bytes_received < 48) {
      out.result = "short_response";
      return;
    }

    // Parse fields. NTP packet is in network byte order (big endian).
    out.stratum = resp_buf[1];

    auto read_be32 = [&](int offset) {
      return (static_cast<std::uint32_t>(resp_buf[offset]) << 24) | (static_cast<std::uint32_t>(resp_buf[offset + 1]) << 16) |
             (static_cast<std::uint32_t>(resp_buf[offset + 2]) << 8) | (static_cast<std::uint32_t>(resp_buf[offset + 3]));
    };

    // Receive timestamp T2 at byte 32, transmit timestamp T3 at byte 40.
    const std::uint32_t t2_secs = read_be32(32);
    const std::uint32_t t2_frac = read_be32(36);
    const std::uint32_t t3_secs = read_be32(40);
    const std::uint32_t t3_frac = read_be32(44);

    const long long t2_unix_ms = ntp_to_unix_ms(t2_secs, t2_frac);
    const long long t3_unix_ms = ntp_to_unix_ms(t3_secs, t3_frac);

    if (t2_unix_ms == 0 || t3_unix_ms == 0) {
      out.result = "no_timestamp";
      return;
    }

    // offset = ((T2 - T1) + (T3 - T4)) / 2  (server - local)
    // We store (local - server) so positive = local ahead.
    const long long server_minus_local = ((t2_unix_ms - t1_ms) + (t3_unix_ms - t4_ms)) / 2;
    out.offset_ms = -server_minus_local;

    if (out.stratum == 0 || out.stratum >= 16) {
      out.result = "kiss_of_death";
    } else {
      out.result = "ok";
    }
  } catch (const std::exception &e) {
    out.result = std::string("error: ") + e.what();
  }

  boost::system::error_code ignore;
  socket.close(ignore);
}

}  // namespace

void check_ntp_offset(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using check_ntp_filter::filter;
  using check_ntp_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  std::vector<std::string> servers;
  std::string servers_string;
  unsigned short port = 123;
  int timeout_ms = 5000;

  filter f;
  filter_helper.add_options("offset > 60000 or stratum >= 16", "offset > 120000 or stratum >= 16", "", f.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${problem_list}", "${server} offset=${offset_signed}ms stratum=${stratum}", "${server}", "No NTP server checked",
                           "%(status): All %(count) NTP servers in sync");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("server", po::value<std::vector<std::string> >(&servers),
        "NTP server to query (may be given multiple times).")
    ("servers", po::value<std::string>(&servers_string),
        "Comma separated list of NTP servers to query.")
    ("port", po::value<unsigned short>(&port)->default_value(123), "UDP port to use (default: 123).")
    ("timeout", po::value<int>(&timeout_ms)->default_value(5000), "Timeout in milliseconds.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (!servers_string.empty()) {
    std::vector<std::string> tmp;
    boost::split(tmp, servers_string, boost::is_any_of(","));
    for (auto &s : tmp) {
      boost::trim(s);
      if (!s.empty()) servers.push_back(s);
    }
  }

  if (servers.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No NTP server specified");

  if (!filter_helper.build_filter(f)) return;

  for (const auto &server : servers) {
    auto obj = boost::make_shared<filter_obj>();
    run_ntp_check(server, port, timeout_ms, *obj);
    f.match(obj);
  }

  filter_helper.post_process(f);
}

}  // namespace check_net
