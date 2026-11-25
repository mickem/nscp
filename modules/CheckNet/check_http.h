// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <boost/optional.hpp>
#include <cmath>
#include <map>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

#include "check_net_cert.hpp"

namespace check_net {
namespace check_http_filter {

struct filter_obj {
  std::string url;
  std::string host;
  long long port;
  std::string path;
  std::string protocol;
  long long status_code;
  long long time;
  long long size;
  // The served certificate. cert.has_certificate is the guard: the expiry is
  // legitimately negative for an expired certificate, so "plain http, or a
  // peer that presented none" cannot be encoded as a value.
  cert::cert_fields cert;
  std::string status_message;
  std::string body;
  std::string result;
  // Values extracted from the JSON response body via --json-path, keyed by alias.
  std::map<std::string, double> json_numbers;
  std::map<std::string, std::string> json_strings;

  filter_obj() : port(0), status_code(0), time(0), size(0) {}

  std::string show() const { return url + " (" + std::to_string(status_code) + ", " + result + ")"; }

  // Empty when the path is missing, null, or holds a string, array or object:
  // that is "no value", not 0, so `alias < 5` cannot fire on it.
  //
  // The number comes from the remote server, so it is bounded to the 64-bit
  // integer range before it is compared or graphed: 1e300 (or the infinity an
  // overflowing exponent parses to) still trips `alias > 100`, but must not
  // reach the perfdata as a 300-digit value or as "inf". The fraction of an
  // in-range value is untouched.
  boost::optional<double> get_json_number(const std::string &alias) const {
    const auto it = json_numbers.find(alias);
    if (it == json_numbers.end() || std::isnan(it->second)) return boost::none;
    const double limit = 9223372036854775807.0;
    return std::max(-limit, std::min(limit, it->second));
  }
  // The text a json-path alias renders and compares as: the extracted value,
  // or "missing" when the path is missing or null (or the body is not JSON),
  // so `alias = 'missing'` catches a field that has disappeared.
  std::string get_json_string(const std::string &alias) const {
    const auto it = json_strings.find(alias);
    return it != json_strings.end() ? it->second : "missing";
  }

  std::string get_url() const { return url; }
  std::string get_host() const { return host; }
  long long get_port() const { return port; }
  std::string get_path() const { return path; }
  std::string get_protocol() const { return protocol; }
  long long get_code() const { return status_code; }
  long long get_time() const { return time; }
  long long get_size() const { return size; }
  long long get_ssl_expiry_days() const { return cert.expiry_days.get_value_or(-1); }
  boost::optional<long long> get_ssl_expiry_days_opt() const { return cert.expiry_days; }
  std::string get_status() const { return status_message; }
  std::string get_body() const { return body; }
  std::string get_result() const { return result; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_http_filter

void check_http(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check_net
