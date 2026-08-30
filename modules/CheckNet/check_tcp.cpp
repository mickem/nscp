// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_tcp.h"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#ifdef USE_SSL
#include <boost/asio/ssl.hpp>
#endif
#include <boost/chrono.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <chrono>
#include <memory>
#include <net/address_family.hpp>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>

#include "check_net_error.hpp"

namespace po = boost::program_options;

namespace check_net {
namespace check_tcp_filter {

filter_obj_handler::filter_obj_handler() {
  register_common_keywords(registry_);

  // TLS certificate of the connected peer. Registered here rather than in the
  // common set because check_ssh is never TLS and would only advertise a
  // keyword that can never be populated.
  registry_
      .add_optional_int_var("ssl_expiry_days", [](auto obj) { return obj->get_ssl_expiry_days_opt(); }, "no certificate",
                            "Whole days until the peer's TLS certificate expires; negative once it has expired. Renders as 'no certificate' (and compares "
                            "false against every number) when the connection is not TLS or the peer presented none, so `ssl_expiry_days < 30` cannot fire "
                            "on a plain connection; `ssl_expiry_days = 'no certificate'` tests for that state.")
      .add_int_perf("", "", "_ssl_expiry_days");
  registry_
      .add_int_var("has_certificate", parsers::where::type_int, &filter_obj::get_has_certificate,
                   "1 when the peer presented a TLS certificate, 0 otherwise")
      .no_perf();
}

}  // namespace check_tcp_filter

namespace check_ssh_filter {

filter_obj_handler::filter_obj_handler() {
  check_tcp_filter::register_common_keywords(registry_);

  // Fields parsed out of the SSH identification string. They are empty (and the
  // numeric ones 0) whenever no banner was read — a refused/timed-out
  // connection, or a port that is not speaking SSH — so guard on `result` when
  // that distinction matters.
  registry_.add_string_var("banner", &filter_obj::get_banner, "The raw SSH identification string, e.g. SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13.5");
  registry_.add_string_var("protocol", &filter_obj::get_protocol, "SSH protocol version the server announced, e.g. 2.0 or 1.99");
  registry_.add_string_var("version", &filter_obj::get_version, "Software version the server announced, e.g. OpenSSH_9.6p1");
  registry_.add_string_var("software", &filter_obj::get_software, "Software name from the version string, e.g. OpenSSH or dropbear");
  registry_.add_string_var("software_version", &filter_obj::get_software_version, "Software version number from the version string, e.g. 9.6p1 or 2022.83");
  registry_.add_string_var("comments", &filter_obj::get_comments, "Trailing comments of the identification string, e.g. the distribution patch level");
  registry_.add_int_var("protocol_major", parsers::where::type_int, &filter_obj::get_protocol_major,
                        "Major SSH protocol version as a number (2 for 2.0); use protocol_major < 2 to catch an SSHv1-only server")
      .no_perf();
  registry_
      .add_int_var("protocol_minor", parsers::where::type_int, &filter_obj::get_protocol_minor,
                   "Minor SSH protocol version as a number (0 for 2.0, 99 for 1.99)")
      .no_perf();
}

}  // namespace check_ssh_filter

namespace {

using boost::asio::ip::tcp;

// Send the optional payload then read the peer's response (with a deadline) and
// apply the expect / expect_regex matchers. Works over any stream that supports
// boost::asio::write / async_read (a plain tcp::socket or a TLS stream). `lowest`
// is the underlying socket, closed to unblock a read that overruns the timeout.
template <typename Stream>
void tcp_converse(Stream &stream, tcp::socket &lowest, boost::asio::io_context &io_service, int timeout_ms, const std::string &send_data,
                  const std::string &expect, const std::string &expect_regex, check_tcp_filter::filter_obj &out) {
  if (!send_data.empty()) {
    boost::system::error_code write_ec;
    boost::asio::write(stream, boost::asio::buffer(send_data), write_ec);
    if (write_ec) {
      out.result = "write_failed";
      return;
    }
  }

  if (expect.empty() && expect_regex.empty()) return;

  // Read whatever the peer sends within a small window, with a deadline.
  boost::asio::streambuf response_buf;
  boost::system::error_code read_ec = boost::asio::error::would_block;
  bool read_done = false;
  boost::asio::steady_timer timer(io_service);

  timer.expires_after(std::chrono::milliseconds(timeout_ms));
  timer.async_wait([&](const boost::system::error_code &ec) {
    if (!ec && !read_done) {
      boost::system::error_code ignore;
      lowest.close(ignore);
    }
  });

  boost::asio::async_read(stream, response_buf, boost::asio::transfer_at_least(1), [&](const boost::system::error_code &ec, std::size_t) {
    read_ec = ec;
    read_done = true;
    // cancel() can throw (the non-throwing cancel(ec) overload is removed under
    // BOOST_ASIO_NO_DEPRECATED). Swallow it so an incidental failure can't
    // escape this handler and misreport a successful read.
    try {
      timer.cancel();
    } catch (...) {
    }
  });

  io_service.run();

  // A peer that closes cleanly reports eof; a TLS peer that closes without a
  // close_notify reports stream_truncated. Both mean "no more data" — evaluate
  // whatever we received rather than failing the check.
  bool clean_end = (read_ec == boost::asio::error::eof);
#ifdef USE_SSL
  if (read_ec == boost::asio::ssl::error::stream_truncated) clean_end = true;
#endif
  if (read_ec && !clean_end) {
    out.result = "read_failed";
    return;
  }

  const std::string data{boost::asio::buffers_begin(response_buf.data()), boost::asio::buffers_end(response_buf.data())};
  out.response = boost::trim_copy(data);

  bool matched = true;
  if (!expect.empty() && data.find(expect) == std::string::npos) matched = false;
  if (matched && !expect_regex.empty()) {
    try {
      if (!boost::regex_search(data, boost::regex(expect_regex))) matched = false;
    } catch (const std::exception &) {
      out.result = "error: invalid expect regex";
      return;
    }
  }
  out.result = matched ? "ok" : "no_match";
}

// Synchronous TCP connect with millisecond timeout. When use_tls is set a TLS
// handshake is performed after the TCP connect. Optionally writes "send_data"
// and reads the response; the raw response is stored in out.response (trimmed).
// If "expect" (substring) and/or "expect_regex" are given the result is set to
// "no_match" when the response fails either. "result" gets a short status word
// (ok/timeout/refused/no_match/tls_handshake_failed/error).
void run_tcp_check(const std::string &host, unsigned short port, int timeout_ms, const std::string &send_data, const std::string &expect,
                   const std::string &expect_regex, bool use_tls, const std::string &tls_version, const std::string &verify_mode, const std::string &ca_file,
                   net::address_family af, check_tcp_filter::filter_obj &out) {
  out.host = host;
  out.port = port;
  out.connected = false;
  out.result = "error";
  out.time = 0;

  boost::asio::io_context io_service;
  tcp::resolver resolver(io_service);
  tcp::socket socket(io_service);
  boost::asio::steady_timer timer(io_service);

  const auto start = boost::chrono::steady_clock::now();
  boost::system::error_code connect_ec = boost::asio::error::would_block;

  try {
    bool connect_done = false;
    boost::system::error_code resolve_ec;
    auto endpoints = net::resolve_for_family(resolver, af, host, std::to_string(port), resolve_ec);
    if (resolve_ec || endpoints.empty()) {
      // With address-family pinned this also covers "the name exists but has no
      // address in the requested family", which is the answer the check is
      // being asked for rather than an internal error.
      out.result = "resolve_failed";
      return;
    }

    timer.expires_after(std::chrono::milliseconds(timeout_ms));
    timer.async_wait([&](const boost::system::error_code &ec) {
      if (!ec && !connect_done) {
        boost::system::error_code ignore;
        socket.close(ignore);
      }
    });

    boost::asio::async_connect(socket, endpoints, [&](const boost::system::error_code &ec, const tcp::endpoint &) {
      connect_ec = ec;
      connect_done = true;
      // cancel() can throw (the non-throwing cancel(ec) overload is removed
      // under BOOST_ASIO_NO_DEPRECATED). Swallow it so an incidental failure
      // can't escape this handler and misreport a successful connect.
      try {
        timer.cancel();
      } catch (...) {
      }
    });

    io_service.run();
    io_service.restart();

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

    if (!use_tls) {
      tcp_converse(socket, socket, io_service, timeout_ms, send_data, expect, expect_regex, out);
    } else {
#ifdef USE_SSL
      boost::asio::ssl::context ctx(socket_helpers::tls_method_parser(tls_version));
      // A "1.2+" tls version resolves to the generic method plus a floor that
      // has to be applied separately, or the '+' silently means "any".
      socket_helpers::apply_tls_min_version(ctx, tls_version);
      if (!ca_file.empty() && ca_file != "none") {
        try {
          ctx.load_verify_file(ca_file);
        } catch (const std::exception &e) {
          out.result = std::string("error: failed to load CA: ") + e.what();
          return;
        }
      }
      // Wrap the already-connected socket by reference so we keep the timed
      // connect above and only layer TLS on top.
      boost::asio::ssl::stream<tcp::socket &> ssl_stream(socket, ctx);
      const boost::asio::ssl::verify_mode vmode = socket_helpers::verify_mode_parser(verify_mode);
      ssl_stream.set_verify_mode(vmode);
      if (!host.empty()) SSL_set_tlsext_host_name(ssl_stream.native_handle(), host.c_str());
      if (vmode != boost::asio::ssl::verify_none) ssl_stream.set_verify_callback(boost::asio::ssl::host_name_verification(host));

      // Handshake with the same millisecond deadline as the connect.
      boost::system::error_code hs_ec = boost::asio::error::would_block;
      bool hs_done = false;
      timer.expires_after(std::chrono::milliseconds(timeout_ms));
      timer.async_wait([&](const boost::system::error_code &ec) {
        if (!ec && !hs_done) {
          boost::system::error_code ignore;
          socket.close(ignore);
        }
      });
      ssl_stream.async_handshake(boost::asio::ssl::stream_base::client, [&](const boost::system::error_code &ec) {
        hs_ec = ec;
        hs_done = true;
        try {
          timer.cancel();
        } catch (...) {
        }
      });
      io_service.run();
      io_service.restart();

      if (hs_ec) {
        out.result = "tls_handshake_failed";
        return;
      }

      // Read the peer certificate straight after the handshake: it is available
      // regardless of `verify`, so an expiry check needs no trust decision.
      if (const auto expiry = socket_helpers::peer_certificate_expiry_days(ssl_stream.native_handle())) {
        out.has_certificate = true;
        out.ssl_expiry_days = *expiry;
      }

      tcp_converse(ssl_stream, socket, io_service, timeout_ms, send_data, expect, expect_regex, out);
#else
      out.result = "error: TLS requested but this build has no TLS support";
      return;
#endif
    }
  } catch (const std::exception &e) {
    out.result = std::string("error: ") + check_net::format_exception_message(e);
  }

  boost::system::error_code ignore;
  socket.close(ignore);
}

}  // namespace

namespace {
// Shared core for check_tcp and check_ssh. When `forced` is non-null (check_ssh)
// its preset is always applied; otherwise the preset is chosen from a `service`
// argument. FilterT/ObjT let check_ssh plug in its own object, which adds the
// parsed identification string on top of the TCP fields.
template <typename FilterT, typename ObjT>
void check_tcp_impl(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                    const service_preset *forced) {
  typedef FilterT filter;
  typedef ObjT filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  std::vector<std::string> hosts;
  std::string hosts_string;
  unsigned short port = 0;
  int timeout_ms = 5000;
  std::string send_data;
  std::string expect;
  std::string service;
  bool use_ssl = false;
  std::string tls_version = "tlsv1.2+";
  std::string verify_mode = "none";
  std::string ca_file;
  std::string address_family_arg;

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
    ("expect", po::value<std::string>(&expect), "Optional substring expected in the response.")
    ("ssl", po::value<bool>(&use_ssl)->implicit_value(true)->default_value(false), "Wrap the connection in TLS/SSL after connecting (ssl=true).")
    ("tls-version", po::value<std::string>(&tls_version)->default_value("tlsv1.2+"),
        "TLS version when --ssl is used (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).")
    ("verify", po::value<std::string>(&verify_mode)->default_value("none"),
        "Certificate verify mode when --ssl is used: none (default), peer, ... (peer requires --ca).")
    ("ca", po::value<std::string>(&ca_file), "CA bundle used to verify the server certificate when --ssl --verify peer is used.")
    ("address-family", po::value<std::string>(&address_family_arg), net::address_family_option_help())
    ;
  if (forced == nullptr) {
    filter_helper.get_desc().add_options()
      ("service", po::value<std::string>(&service),
          "Service preset (ftp, pop, imap, smtp, ssh, spop, simap, ssmtp): sets a default port, greeting and expected-response regex. "
          "The s-prefixed variants use implicit TLS.")
      ;
  }
  // clang-format on

  if (!filter_helper.parse_options()) return;

  net::address_family af = net::address_family::any;
  if (!net::parse_address_family(address_family_arg, af))
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid address-family: " + address_family_arg + " (expected any, ipv4 or ipv6)");

  // Resolve the preset: forced (check_ssh) or from the `service` argument.
  const service_preset *preset = forced;
  if (preset == nullptr && !service.empty()) {
    preset = find_service_preset(service);
    if (preset == nullptr) return nscapi::protobuf::functions::set_response_bad(*response, "Unknown service preset: " + service);
  }

  std::string expect_regex;
  if (preset != nullptr) {
    if (port == 0) port = preset->port;
    if (send_data.empty()) send_data = preset->send;
    expect_regex = preset->expect_regex;
    if (preset->tls) use_ssl = true;
  }

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
    auto obj = std::make_shared<filter_obj>();
    run_tcp_check(host, port, timeout_ms, send_data, expect, expect_regex, use_ssl, tls_version, verify_mode, ca_file, af, *obj);
    obj->post_read();
    f.match(obj);
  }

  filter_helper.post_process(f);
}
}  // namespace

void check_tcp(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_tcp_impl<check_tcp_filter::filter, check_tcp_filter::filter_obj>(request, response, nullptr);
}

void check_ssh(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_tcp_impl<check_ssh_filter::filter, check_ssh_filter::filter_obj>(request, response, find_service_preset("SSH"));
}

}  // namespace check_net
