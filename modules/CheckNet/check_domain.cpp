// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_domain.hpp"

#include <boost/program_options.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>

#include "check_net_error.hpp"

namespace check_net {
namespace domain {
filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("domain", &filter_obj::get_domain, "Registered domain name (ASCII/punycode).");
  registry_.add_string_var("expiration", &filter_obj::get_expiration, "Selected expiration timestamp in UTC.");
  registry_.add_string_var("expiration_type", &filter_obj::get_expiration_type, "Date source: registrar, registry or whois.");
  registry_.add_string_var("source", &filter_obj::get_source, "RDAP URL or WHOIS server that supplied the selected date.");
  registry_
      .add_int_var("expires_in", parsers::where::type_int, &filter_obj::get_expires_in, "Whole days until expiration, rounded down; negative once expired.")
      .add_int_perf("d");
}
}  // namespace domain

void check_domain(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                  PB::Commands::QueryResponseMessage::Response *response) {
  namespace po = boost::program_options;
  modern_filter::data_container data;
  modern_filter::cli_helper<domain::filter> helper(request, response, data);
  domain::filter filter;
  domain::lookup_options options;
  options.ca_file = default_ca_file;
  std::string name;
  helper.add_options("expires_in < 30", "expires_in < 10", "", filter.get_filter_syntax(), "unknown");
  helper.add_syntax("${status}: ${problem_list}", "${domain} expires in ${expires_in}d (${expiration}, ${expiration_type}, ${source})", "${domain}",
                    "No domain checked", "%(status): %(list)");
  // clang-format off
  helper.get_desc().add_options()
    ("domain", po::value<std::string>(&name), "Registered domain to check, not a URL or subdomain. Use punycode for internationalized names.")
    ("rdap-url", po::value<std::string>(&options.rdap_url)->default_value(options.rdap_url),
      "HTTPS domain lookup URL; {domain} is replaced with the normalized domain. Redirects and one related registrar record are followed.")
    ("ca", po::value<std::string>(&options.ca_file)->default_value(default_ca_file), "CA bundle for verified HTTPS (defaults to the system CA bundle).")
    ("timeout", po::value<int>(&options.timeout)->default_value(15), "Timeout in seconds for each network read or write, including WHOIS fallback. Not an overall lookup deadline.")
    ("whois-fallback", po::value<bool>(&options.whois_fallback)->implicit_value(true)->default_value(false),
      "Try the explicitly configured WHOIS server if RDAP fails or has no expiration. WHOIS is unencrypted and unauthenticated.")
    ("whois-server", po::value<std::string>(&options.whois_server), "WHOIS server to use with whois-fallback=true. Referrals are not followed.")
    ("whois-port", po::value<unsigned short>(&options.whois_port)->default_value(43), "WHOIS TCP port.")
    ;
  // clang-format on
  if (!helper.parse_options()) return;
  if (options.timeout < 1 || options.timeout > 300)
    return nscapi::protobuf::functions::set_response_bad(*response, "timeout must be between 1 and 300 seconds");
  if (options.whois_fallback && (options.whois_server.empty() || options.whois_port == 0))
    return nscapi::protobuf::functions::set_response_bad(*response, "whois-fallback requires whois-server and a nonzero whois-port");
  if (!helper.build_filter(filter)) return;
  try {
    name = domain::normalize_domain(name);
    filter.match(std::make_shared<domain::filter_obj>(domain::lookup(name, options)));
  } catch (const std::exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Domain lookup failed: " + check_net::format_exception_message(e));
  }
  helper.post_process(filter);
}
}  // namespace check_net
