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

#include "check_dns.h"

#include "check_net_error.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>

#include <set>

namespace po = boost::program_options;

namespace check_net {
namespace check_dns_filter {

filter_obj_handler::filter_obj_handler() {
  registry_.add_string("host", &filter_obj::get_host, "Hostname that was looked up");
  registry_.add_string("addresses", &filter_obj::get_addresses, "Comma separated list of resolved addresses");
  registry_.add_string("result", &filter_obj::get_result, "Textual result of the lookup (ok, not_found, mismatch, error, ...)");
  registry_.add_int_x("count", parsers::where::type_int, &filter_obj::get_count, "Number of addresses returned by the resolver");
  registry_.add_int_x("time", parsers::where::type_int, &filter_obj::get_time, "Time taken by the lookup in milliseconds");
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

  boost::asio::io_service io_service;
  tcp::resolver resolver(io_service);
  boost::asio::deadline_timer timer(io_service);

  bool resolve_done = false;
  boost::system::error_code resolve_ec = boost::asio::error::would_block;
  std::vector<std::string> addrs;

  const auto start = boost::chrono::steady_clock::now();

  timer.expires_from_now(boost::posix_time::milliseconds(timeout_ms));
  timer.async_wait([&](const boost::system::error_code &ec) {
    if (!ec && !resolve_done) {
      resolver.cancel();
    }
  });

  try {
    tcp::resolver::query query(host, "");
    resolver.async_resolve(query, [&](const boost::system::error_code &ec, tcp::resolver::iterator it) {
      resolve_done = true;
      resolve_ec = ec;
      const tcp::resolver::iterator end;
      std::set<std::string> seen;
      while (it != end) {
        const std::string a = it->endpoint().address().to_string();
        if (seen.insert(a).second) addrs.push_back(a);
        ++it;
      }
      boost::system::error_code ignore;
      timer.cancel(ignore);
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

  filter f;
  filter_helper.add_options("time > 1000", "result != 'ok'", "", f.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${problem_list}", "${host} -> ${addresses} (${count}) in ${time}ms [${result}]", "${host}", "No DNS lookup performed",
                           "%(status): %(list)");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("host", po::value<std::string>(&host), "Hostname to look up.")
    ("lookup", po::value<std::string>(&host), "Alias for --host.")
    ("timeout", po::value<int>(&timeout_ms)->default_value(5000), "Timeout in milliseconds.")
    ("expected-address", po::value<std::vector<std::string> >(&expected),
        "Address that must be present in the answer (may be given multiple times).")
    ("expected", po::value<std::string>(&expected_string),
        "Comma separated list of addresses that must all be present in the answer.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (host.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No host specified");

  if (!expected_string.empty()) {
    std::vector<std::string> tmp;
    boost::split(tmp, expected_string, boost::is_any_of(","));
    for (auto &e : tmp) {
      boost::trim(e);
      if (!e.empty()) expected.push_back(e);
    }
  }

  if (!filter_helper.build_filter(f)) return;

  auto obj = boost::make_shared<filter_obj>();
  run_dns_check(host, timeout_ms, expected, *obj);
  f.match(obj);

  filter_helper.post_process(f);
}

}  // namespace check_net
