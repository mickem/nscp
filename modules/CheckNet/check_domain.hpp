// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace check_net {
namespace domain {

struct registration {
  boost::posix_time::ptime expiration;
  std::string expiration_type;
  std::string registrar_url;
};

// Throws on ambiguous or unusable input. Only ASCII DNS names are accepted;
// internationalized names must be supplied as their IDNA A-label (xn--...).
std::string normalize_domain(std::string name);
boost::posix_time::ptime parse_timestamp(const std::string &value);
long long days_remaining(const boost::posix_time::ptime &expiration, const boost::posix_time::ptime &now);
registration parse_rdap(const std::string &body, const std::string &domain);
registration parse_whois(const std::string &body, const std::string &domain);

struct lookup_options {
  std::string rdap_url = "https://rdap.org/domain/{domain}";
  std::string ca_file;
  int timeout = 15;  // Per-operation read/write timeout in seconds.
  bool whois_fallback = false;
  std::string whois_server;
  unsigned short whois_port = 43;
};

struct filter_obj {
  std::string domain;
  std::string expiration;
  std::string expiration_type;
  std::string source;
  long long expires_in = 0;
  std::string show() const { return domain + " expires in " + std::to_string(expires_in) + "d"; }
  std::string get_domain() const { return domain; }
  std::string get_expiration() const { return expiration; }
  std::string get_expiration_type() const { return expiration_type; }
  std::string get_source() const { return source; }
  long long get_expires_in() const { return expires_in; }
};
typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

filter_obj lookup(const std::string &name, const lookup_options &options);
}  // namespace domain

void check_domain(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                  PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check_net
