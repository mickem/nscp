// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_ntp_offset.h"

#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include <boost/program_options.hpp>
#include <vector>
#include <net/address_family.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>

#include "check_net_error.hpp"
#include "check_ntp_internal.hpp"

namespace po = boost::program_options;

namespace check_net {
namespace check_ntp_filter {

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("server", &filter_obj::get_server, "NTP server that was queried");
  registry_.add_string_var("result", &filter_obj::get_result, "Textual result of the query (ok, timeout, error, ...)");
  registry_.add_int_var("port", parsers::where::type_int, &filter_obj::get_port, "UDP port the query was sent to");
  registry_.add_int_var("offset", parsers::where::type_int, &filter_obj::get_offset, "Absolute clock offset between local host and server, in milliseconds")
      .add_int_perf("ms");
  registry_.add_int_var("offset_signed", parsers::where::type_int, &filter_obj::get_offset_signed,
                        "Signed clock offset (positive = local clock is ahead of server), in milliseconds");
  registry_.add_int_var("stratum", parsers::where::type_int, &filter_obj::get_stratum, "Stratum reported by the server (0..16)").add_int_perf("");
  registry_.add_int_var("time", parsers::where::type_int, &filter_obj::get_time, "Round trip time of the NTP query in milliseconds").add_int_perf("ms");

  // How steady the server's time is, rather than how far off it is: a source
  // can answer promptly with an accurate-looking offset and still be unusable
  // because that offset will not hold still.
  registry_
      .add_int_var("jitter", parsers::where::type_int, &filter_obj::get_jitter,
                   "RMS variation between the sampled offsets, in milliseconds; -1 when fewer than 2 samples were taken (raise samples= to measure it)")
      .add_int_perf("ms", "", "_jitter");
  registry_.add_int_var("samples", parsers::where::type_int, &filter_obj::get_samples, "Number of samples that answered").no_perf();
  registry_
      .add_int_var("root_delay", parsers::where::type_int, &filter_obj::get_root_delay,
                   "Round trip delay the server reports to its own reference clock, in milliseconds")
      .add_int_perf("ms", "", "_root_delay");
  registry_
      .add_int_var("root_dispersion", parsers::where::type_int, &filter_obj::get_root_dispersion,
                   "Maximum error the server claims for the time it is serving, in milliseconds")
      .add_int_perf("ms", "", "_root_dispersion");
}

}  // namespace check_ntp_filter

namespace {

using check_ntp_internal::ntp_offset_ms;
using check_ntp_internal::ntp_to_unix_ms;

// One request/response exchange. Fills `out` with the result of that single
// sample; run_ntp_check below repeats it when more than one sample is asked
// for.
void run_ntp_query(const std::string &server, unsigned short port, int timeout_ms, net::address_family af, check_ntp_filter::filter_obj &out) {
  using boost::asio::ip::udp;

  out.server = server;
  out.port = port;
  out.result = "error";
  out.offset_ms = 0;
  out.stratum = 0;
  out.time = 0;
  out.root_delay = 0;
  out.root_dispersion = 0;

  boost::asio::io_context io_service;
  udp::resolver resolver(io_service);
  udp::socket socket(io_service);
  boost::asio::steady_timer timer(io_service);

  try {
    boost::system::error_code resolve_ec;
    auto results = net::resolve_for_family(resolver, af, server, std::to_string(port), resolve_ec);
    if (resolve_ec || results.empty()) {
      out.result = "resolve_failed";
      return;
    }
    const udp::endpoint endpoint = results.begin()->endpoint();

    // Open the socket in whatever family the server actually resolved to. This
    // used to be hardcoded to v4, which made an IPv6 NTP server unreachable.
    socket.open(endpoint.protocol());

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

    timer.expires_after(std::chrono::milliseconds(timeout_ms));
    timer.async_wait([&](const boost::system::error_code &ec) {
      if (!ec && !recv_done) {
        boost::system::error_code ignore;
        socket.close(ignore);
      }
    });

    socket.async_receive_from(boost::asio::buffer(resp_buf, sizeof(resp_buf)), sender, [&](const boost::system::error_code &ec, std::size_t n) {
      recv_ec = ec;
      bytes_received = n;
      recv_done = true;
      // cancel() can throw (the non-throwing cancel(ec) overload is removed
      // under BOOST_ASIO_NO_DEPRECATED). Swallow it so an incidental failure
      // can't escape this handler and misreport a successful receive.
      try {
        timer.cancel();
      } catch (...) {
      }
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

    // Root delay (byte 4) and root dispersion (byte 8) are "NTP short"
    // fixed-point seconds: what the server itself claims about the quality of
    // the time it is serving, independent of our own measurement.
    out.root_delay = check_ntp_internal::ntp_short_to_ms(read_be32(4));
    out.root_dispersion = check_ntp_internal::ntp_short_to_ms(read_be32(8));

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
    const long long server_minus_local = ntp_offset_ms(t1_ms, t2_unix_ms, t3_unix_ms, t4_ms);
    out.offset_ms = -server_minus_local;

    if (out.stratum == 0 || out.stratum >= 16) {
      out.result = "kiss_of_death";
    } else {
      out.result = "ok";
    }
  } catch (const std::exception &e) {
    out.result = std::string("error: ") + check_net::format_exception_message(e);
  }

  boost::system::error_code ignore;
  socket.close(ignore);
}

// Query the server `samples` times and summarise. With the default of one
// sample this is exactly one exchange and behaves as it always has.
//
// Sampling stops at the first failure, so an unreachable or slow server costs
// one timeout rather than `samples` of them.
void run_ntp_check(const std::string &server, unsigned short port, int timeout_ms, net::address_family af, int samples,
                   check_ntp_filter::filter_obj &out) {
  if (samples < 1) samples = 1;

  std::vector<long long> offsets;
  offsets.reserve(static_cast<std::size_t>(samples));
  bool have_best = false;

  for (int i = 0; i < samples; ++i) {
    check_ntp_filter::filter_obj sample;
    run_ntp_query(server, port, timeout_ms, af, sample);

    if (sample.result != "ok") {
      // Report the failure, but keep any good samples already collected so the
      // jitter of a partially-answered burst is not thrown away.
      if (!have_best) out = sample;
      out.result = sample.result;
      break;
    }

    offsets.push_back(sample.offset_ms);
    // Keep the sample with the shortest round trip: a delayed packet biases the
    // offset by roughly half the extra delay, so the quickest exchange is the
    // most trustworthy estimate of the real offset.
    if (!have_best || sample.time < out.time) {
      out = sample;
      have_best = true;
    }
  }

  out.samples = static_cast<long long>(offsets.size());
  out.jitter = check_ntp_internal::rms_jitter_ms(offsets);
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
  int samples = 1;
  std::string address_family_arg;

  filter f;
  filter_helper.add_options("offset > 50 or stratum >= 16", "offset > 100 or stratum >= 16 or result != 'ok'", "", f.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${problem_list}", "${server} offset=${offset_signed}ms stratum=${stratum}", "${server}", "No NTP server checked",
                           "%(status): %(list)");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("server", po::value<std::vector<std::string> >(&servers),
        "NTP server to query (may be given multiple times).")
    ("servers", po::value<std::string>(&servers_string),
        "Comma separated list of NTP servers to query.")
    ("port", po::value<unsigned short>(&port)->default_value(123), "UDP port to use (default: 123).")
    ("timeout", po::value<int>(&timeout_ms)->default_value(5000), "Timeout in milliseconds.")
    ("address-family", po::value<std::string>(&address_family_arg), net::address_family_option_help())
    ("samples", po::value<int>(&samples)->default_value(1),
        "Number of queries to send to each server (default: 1). At least 2 are needed for the jitter keyword, which is the variation between samples; "
        "sampling stops at the first failure so an unreachable server still costs only one timeout.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  net::address_family af = net::address_family::any;
  if (!net::parse_address_family(address_family_arg, af))
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid address-family: " + address_family_arg + " (expected any, ipv4 or ipv6)");

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
    auto obj = std::make_shared<filter_obj>();
    run_ntp_check(server, port, timeout_ms, af, samples, *obj);
    f.match(obj);
  }

  filter_helper.post_process(f);
}

}  // namespace check_net
