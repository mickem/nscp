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

#pragma once

#include <map>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

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
  long long ssl_expiry_days;
  std::string status_message;
  std::string body;
  std::string result;
  // Values extracted from the JSON response body via --json-path, keyed by alias.
  std::map<std::string, double> json_numbers;
  std::map<std::string, std::string> json_strings;

  filter_obj() : port(0), status_code(0), time(0), size(0), ssl_expiry_days(-1) {}

  std::string show() const { return url + " (" + std::to_string(status_code) + ", " + result + ")"; }

  double get_json_number(const std::string &alias) const {
    const auto it = json_numbers.find(alias);
    return it != json_numbers.end() ? it->second : 0.0;
  }
  std::string get_json_string(const std::string &alias) const {
    const auto it = json_strings.find(alias);
    return it != json_strings.end() ? it->second : "";
  }

  std::string get_url() const { return url; }
  std::string get_host() const { return host; }
  long long get_port() const { return port; }
  std::string get_path() const { return path; }
  std::string get_protocol() const { return protocol; }
  long long get_code() const { return status_code; }
  long long get_time() const { return time; }
  long long get_size() const { return size; }
  long long get_ssl_expiry_days() const { return ssl_expiry_days; }
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
