// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckNet.h"

#include <boost/atomic.hpp>
#include <memory>
#include <net/address_family.hpp>
#include <net/pinger.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/settings/helper.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <str/xtos.hpp>

#include "check_connections.h"
#include "check_dns.h"
#include "check_http.h"
#include "check_nsclient_web_online.h"
#include "check_ping_internal.hpp"
#include "check_webserver.h"
#include "check_ntp_offset.h"
#include "check_tcp.h"
#include "filter.hpp"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;
boost::atomic<unsigned short> identifier(0);

bool CheckNet::loadModuleEx(const std::string &, NSCAPI::moduleLoadMode) {
  // Resolve the trusted CA bundle path once, at module load. ${ca-path}
  // expands to ${certificate-path}/windows-ca.pem on Windows (the auto-
  // generated system ROOT bundle) and on unix to the distribution's own bundle,
  // detected at configure time because its location differs per family
  // (CONFIG_CA_PATH). check_http hands this through as the default `ca` so HTTPS
  // checks against public-CA-signed servers validate out of the box.
  default_ca_ = get_core()->expand_path("${ca-path}");
  return true;
}

void CheckNet::check_ping(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<ping_filter::filter> filter_helper(request, response, data);
  std::vector<std::string> hosts;
  std::string payload;
  std::string hosts_string;
  bool total = false;
  int count = 0;
  int timeout = 0;
  int size = 0;
  int ttl = 0;
  std::string address_family_arg;

  ping_filter::filter filter;
  filter_helper.add_options("time > 60 or loss > 5%", "time > 100 or loss > 10%", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${ok_count}/${count} (${problem_list})", "${ip} Packet loss = ${loss}%, RTA = ${time}ms", "${host}", "No hosts found",
                           "%(status): All %(count) hosts are ok");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("host", po::value<std::vector<std::string> >(&hosts), "The host to check (or multiple hosts).")
    ("total", po::value<bool>(&total)->implicit_value(true)->default_value(false), "Include the total of all matching hosts")
    ("hosts", po::value<std::string>(&hosts_string), "The host to check (or multiple hosts).")
    ("count", po::value<int>(&count)->default_value(1), "Number of packets to send.")
    ("timeout", po::value<int>(&timeout)->default_value(500), "Timeout in milliseconds.")
    ("payload", po::value<std::string>(&payload)->default_value("Hello from NSClient++."), "The payload to send in the ping request (default: 'Hello from NSClient++')")
    ("address-family", po::value<std::string>(&address_family_arg), net::address_family_option_help())
    ("size", po::value<int>(&size)->default_value(0),
        "Size of the ICMP payload in bytes (0 keeps the --payload string as-is). The payload is repeated or truncated to reach exactly this many bytes; "
        "the 8 byte ICMP header is on top, so a 1472 byte payload is the largest that fits an untagged 1500 byte MTU over IPv4.")
    ("ttl", po::value<int>(&ttl)->default_value(0),
        "TTL / hop limit to set on outgoing packets (0 keeps the system default). Note the ttl keyword reports the TTL of the reply, which is a different "
        "number: it is what is left of the remote host's own outgoing TTL after the return path.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  net::address_family af = net::address_family::any;
  if (!net::parse_address_family(address_family_arg, af))
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid address-family: " + address_family_arg + " (expected any, ipv4 or ipv6)");

  using check_net::check_ping_internal::kMaxPingPayload;
  if (size < 0 || size > kMaxPingPayload)
    return nscapi::protobuf::functions::set_response_bad(
        *response, "Invalid size: " + str::xtos(size) + " (expected 0-" + str::xtos(kMaxPingPayload) + ")");
  if (ttl < 0 || ttl > 255)
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid ttl: " + str::xtos(ttl) + " (expected 0-255)");

  payload = check_net::check_ping_internal::build_ping_payload(payload, size);

  // `size` is new and caller-controlled up to 64 KB; `count` has never been
  // bounded. Together they decide how many bytes this agent throws at an
  // arbitrary `host`, and whoever can run a check picks all three - over REST
  // that is any caller holding `queries.execute`, which the stock `monitoring`
  // and `client` roles both have. Before this option the payload was a fixed
  // ~22 byte string, so count was the only lever; a 64 KB payload multiplies
  // the traffic a single check can generate by about three thousand.
  //
  // Bound the volume rather than `count` itself: an existing high-count check
  // with the default payload is unaffected, and only the combination that turns
  // the agent into a packet cannon is refused.
  constexpr long long kMaxPingBytesPerHost = 10LL * 1024 * 1024;
  const long long packets = count > 0 ? count : 0;
  const long long total_bytes = packets * static_cast<long long>(payload.size());
  if (total_bytes > kMaxPingBytesPerHost)
    return nscapi::protobuf::functions::set_response_bad(
        *response, "Refusing to send " + str::xtos(total_bytes) + " bytes to each host (count=" + str::xtos(count) + " x payload=" +
                       str::xtos(payload.size()) + " bytes); the limit is " + str::xtos(kMaxPingBytesPerHost) + " bytes per host.");

  if (!hosts_string.empty()) boost::split(hosts, hosts_string, boost::is_any_of(","));

  if (hosts.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No host specified");
  if (hosts.size() == 1) filter_helper.show_all = true;

  if (!filter_helper.build_filter(filter)) return;

  std::shared_ptr<ping_filter::filter_obj> total_obj;
  if (total) total_obj = ping_filter::filter_obj::get_total();

  for (const std::string &host : hosts) {
    result_container result;
    for (int i = 0; i < count; i++) {
      boost::asio::io_context io_service;
      auto id = identifier.fetch_add(1, boost::memory_order_relaxed);
      // A destination with no address in the requested family throws out of the
      // pinger constructor and, as before for any resolve failure, surfaces as
      // an error response for the whole check.
      pinger ping(io_service, result, host.c_str(), timeout, id, payload, af, ttl);
      ping.ping();
      io_service.run();
    }
    auto obj = std::make_shared<ping_filter::filter_obj>(result);
    filter.match(obj);
    if (total_obj) total_obj->add(obj);
  }
  if (total_obj) filter.match(total_obj);
  filter_helper.post_process(filter);
}
void CheckNet::check_tcp(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_net::check_tcp(request, response);
}
void CheckNet::check_ssh(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_net::check_ssh(request, response);
}
void CheckNet::check_dns(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_net::check_dns(request, response);
}
void CheckNet::check_http(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const {
  check_net::check_http(default_ca_, request, response);
}
void CheckNet::check_nsclient_web_online(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const {
  check_net::check_nsclient_web_online(default_ca_, request, response);
}
void CheckNet::check_apache_status(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const {
  check_net::check_apache_status(default_ca_, request, response);
}
void CheckNet::check_nginx_status(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const {
  check_net::check_nginx_status(default_ca_, request, response);
}
void CheckNet::check_phpfpm_status(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const {
  check_net::check_phpfpm_status(default_ca_, request, response);
}
void CheckNet::check_tomcat_status(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const {
  check_net::check_tomcat_status(default_ca_, request, response);
}
void CheckNet::check_connections(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_net::check_connections(request, response);
}
void CheckNet::check_ntp_offset(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_net::check_ntp_offset(request, response);
}
