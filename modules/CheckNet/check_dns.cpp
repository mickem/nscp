// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_dns.h"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <set>

#include <fstream>

#include "check_dns_internal.hpp"
#include "check_net_error.hpp"

namespace po = boost::program_options;

namespace check_net {
namespace check_dns_filter {

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("host", &filter_obj::get_host, "Hostname that was looked up");
  registry_.add_string_var("addresses", &filter_obj::get_addresses, "Comma separated list of resolved records");
  registry_.add_string_var("type", &filter_obj::get_type, "Record type that was queried (A, AAAA, MX, TXT, ...)");
  registry_.add_string_var("server", &filter_obj::get_server, "DNS server used (empty for the system resolver)");
  registry_.add_string_var("result", &filter_obj::get_result, "Textual result of the lookup (ok, not_found, mismatch, error, ...)");
  registry_.add_int_var("count", parsers::where::type_int, &filter_obj::get_count, "Number of records returned by the resolver");
  registry_.add_int_var("time", parsers::where::type_int, &filter_obj::get_time, "Time taken by the lookup in milliseconds");
}

}  // namespace check_dns_filter

namespace {

// Resolve "host" using the system resolver.
// Optionally checks that all "expected" addresses appear in the answer.
void run_dns_check(const std::string &host, int timeout_ms, const std::vector<std::string> &expected, check_dns_filter::filter_obj &out) {
  using boost::asio::ip::tcp;

  out.host = host;
  out.count = 0;
  out.time = 0;
  out.result = "error";
  out.addresses.clear();

  boost::asio::io_context io_service;
  tcp::resolver resolver(io_service);
  boost::asio::steady_timer timer(io_service);

  bool resolve_done = false;
  boost::system::error_code resolve_ec = boost::asio::error::would_block;
  std::vector<std::string> addrs;

  const auto start = boost::chrono::steady_clock::now();

  timer.expires_after(std::chrono::milliseconds(timeout_ms));
  timer.async_wait([&](const boost::system::error_code &ec) {
    if (!ec && !resolve_done) {
      resolver.cancel();
    }
  });

  try {
    resolver.async_resolve(host, "", [&](const boost::system::error_code &ec, const tcp::resolver::results_type &results) {
      resolve_done = true;
      resolve_ec = ec;
      std::set<std::string> seen;
      for (const auto &entry : results) {
        const std::string a = entry.endpoint().address().to_string();
        if (seen.insert(a).second) addrs.push_back(a);
      }
      // cancel() can throw (the non-throwing cancel(ec) overload is removed
      // under BOOST_ASIO_NO_DEPRECATED). Swallow it so an incidental failure
      // can't escape this handler and misreport a successful resolve.
      try {
        timer.cancel();
      } catch (...) {
      }
    });

    io_service.run();
  } catch (const std::exception &e) {
    out.result = std::string("error: ") + check_net::format_exception_message(e);
    return;
  }

  const auto elapsed = boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::steady_clock::now() - start).count();
  out.time = static_cast<long long>(elapsed);

  if (resolve_ec) {
    if (resolve_ec == boost::asio::error::operation_aborted) {
      out.result = "timeout";
    } else if (resolve_ec == boost::asio::error::host_not_found || resolve_ec == boost::asio::error::host_not_found_try_again) {
      out.result = "not_found";
    } else {
      out.result = "error";
    }
    return;
  }

  out.count = static_cast<long long>(addrs.size());
  out.addresses = boost::algorithm::join(addrs, ",");

  if (addrs.empty()) {
    out.result = "not_found";
    return;
  }

  if (!expected.empty()) {
    const std::set<std::string> got(addrs.begin(), addrs.end());
    for (const auto &exp : expected) {
      if (got.find(exp) == got.end()) {
        out.result = "mismatch";
        return;
      }
    }
  }
  out.result = "ok";
}

// Read the first `nameserver` entry from /etc/resolv.conf (Linux default
// resolver). Returns "" if none is found.
std::string default_nameserver() {
  std::ifstream ifs("/etc/resolv.conf");
  std::string line;
  while (std::getline(ifs, line)) {
    boost::trim(line);
    if (line.compare(0, 11, "nameserver ") == 0) {
      std::string ns = line.substr(11);
      boost::trim(ns);
      if (!ns.empty()) return ns;
    }
  }
  return "";
}

// Query `server` over UDP for `host`/`qtype` and fill the result. Used for
// non-A/AAAA record types and for an explicitly chosen server.
void run_dns_udp_check(const std::string &host, int qtype, const std::string &server, unsigned short port, int timeout_ms, bool recursion,
                       const std::vector<std::string> &expected, check_dns_filter::filter_obj &out) {
  using boost::asio::ip::udp;
  namespace dns = check_dns_internal;

  out.host = host;
  out.type = dns::type_to_string(qtype);
  out.server = server;
  out.count = 0;
  out.time = 0;
  out.result = "error";
  out.addresses.clear();

  // A fixed query id is fine for a single synchronous exchange.
  const std::string query = dns::build_query(0x1234, host, qtype, recursion);

  boost::asio::io_context io;
  udp::socket socket(io);
  boost::asio::steady_timer timer(io);
  const auto start = boost::chrono::steady_clock::now();

  try {
    udp::resolver resolver(io);
    boost::system::error_code rec;
    auto endpoints = resolver.resolve(udp::v4(), server, std::to_string(port), rec);
    if (rec || endpoints.empty()) {
      out.result = "server_unresolved";
      return;
    }
    const udp::endpoint dest = *endpoints.begin();
    socket.open(udp::v4());
    socket.send_to(boost::asio::buffer(query), dest);

    std::array<char, 4096> buf{};
    boost::system::error_code recv_ec = boost::asio::error::would_block;
    std::size_t received = 0;
    bool done = false;

    timer.expires_after(std::chrono::milliseconds(timeout_ms));
    timer.async_wait([&](const boost::system::error_code &ec) {
      if (!ec && !done) {
        boost::system::error_code ignore;
        socket.close(ignore);
      }
    });
    socket.async_receive(boost::asio::buffer(buf), [&](const boost::system::error_code &ec, std::size_t n) {
      recv_ec = ec;
      received = n;
      done = true;
      try {
        timer.cancel();
      } catch (...) {
      }
    });
    io.run();

    const auto elapsed = boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::steady_clock::now() - start).count();
    out.time = static_cast<long long>(elapsed);

    if (recv_ec == boost::asio::error::operation_aborted) {
      out.result = "timeout";
      return;
    }
    if (recv_ec) {
      out.result = "error";
      return;
    }

    const dns::dns_result parsed = dns::parse_response(std::string(buf.data(), received), qtype);
    if (!parsed.ok) {
      out.result = "malformed";
      return;
    }
    if (parsed.rcode == 3) {
      out.result = "not_found";
      return;
    }
    if (parsed.rcode != 0) {
      out.result = "error";
      return;
    }
    out.count = static_cast<long long>(parsed.records.size());
    out.addresses = boost::algorithm::join(parsed.records, ",");
    if (parsed.records.empty()) {
      out.result = "not_found";
      return;
    }
    if (!expected.empty()) {
      const std::set<std::string> got(parsed.records.begin(), parsed.records.end());
      for (const auto &exp : expected) {
        if (got.find(exp) == got.end()) {
          out.result = "mismatch";
          return;
        }
      }
    }
    out.result = "ok";
  } catch (const std::exception &e) {
    out.result = std::string("error: ") + check_net::format_exception_message(e);
  }

  boost::system::error_code ignore;
  socket.close(ignore);
}

}  // namespace

void check_dns(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using check_dns_filter::filter;
  using check_dns_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  std::string host;
  int timeout_ms = 5000;
  std::vector<std::string> expected;
  std::string expected_string;
  std::string type = "A";
  std::string server;
  unsigned short port = 53;
  bool norec = false;

  filter f;
  filter_helper.add_options("time > 1000", "result != 'ok'", "", f.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${problem_list}", "${host} -> ${addresses} (${count}) in ${time}ms [${result}]", "${host}", "No DNS lookup performed",
                           "%(status): %(list)");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("host", po::value<std::string>(&host), "Hostname to look up.")
    ("lookup", po::value<std::string>(&host), "Alias for --host.")
    ("type", po::value<std::string>(&type)->default_value("A"), "DNS record type to query: A, AAAA, MX, TXT, CNAME, NS, SOA, PTR.")
    ("server", po::value<std::string>(&server), "DNS server to query (default: the system resolver for A/AAAA, /etc/resolv.conf otherwise).")
    ("port", po::value<unsigned short>(&port)->default_value(53), "UDP port of the DNS server.")
    ("norecursion", po::value<bool>(&norec)->implicit_value(true)->default_value(false), "Do not request recursion (RD=0).")
    ("timeout", po::value<int>(&timeout_ms)->default_value(5000), "Timeout in milliseconds.")
    ("expected-address", po::value<std::vector<std::string> >(&expected),
        "Record that must be present in the answer (may be given multiple times).")
    ("expected", po::value<std::string>(&expected_string),
        "Comma separated list of records that must all be present in the answer.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (host.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No host specified");

  const int qtype = check_dns_internal::type_from_string(type);
  if (qtype < 0) return nscapi::protobuf::functions::set_response_bad(*response, "Invalid DNS record type: " + type);

  if (!expected_string.empty()) {
    std::vector<std::string> tmp;
    boost::split(tmp, expected_string, boost::is_any_of(","));
    for (auto &e : tmp) {
      boost::trim(e);
      if (!e.empty()) expected.push_back(e);
    }
  }

  if (!filter_helper.build_filter(f)) return;

  auto obj = std::make_shared<filter_obj>();
  const bool a_or_aaaa = (qtype == check_dns_internal::DNS_A || qtype == check_dns_internal::DNS_AAAA);
  if (server.empty() && a_or_aaaa) {
    // A/AAAA without an explicit server: the system resolver (honours
    // /etc/hosts and nsswitch; may return both families).
    run_dns_check(host, timeout_ms, expected, *obj);
    obj->type = type;
  } else {
    std::string dns_server = server;
    if (dns_server.empty()) dns_server = default_nameserver();
    if (dns_server.empty()) {
      return nscapi::protobuf::functions::set_response_bad(*response, "No DNS server configured; specify one with server=<ip>");
    }
    run_dns_udp_check(host, qtype, dns_server, port, timeout_ms, !norec, expected, *obj);
  }
  f.match(obj);

  filter_helper.post_process(f);
}

}  // namespace check_net
