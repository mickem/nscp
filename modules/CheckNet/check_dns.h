// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace check_net {
namespace check_dns_filter {

struct filter_obj {
  std::string host;
  std::string addresses;
  std::string type;    // queried record type (A, AAAA, MX, TXT, ...)
  std::string server;  // resolver used (empty for the system resolver)
  long long count;
  long long time;
  std::string result;

  filter_obj() : count(0), time(0) {}

  std::string show() const { return host + " -> " + addresses + " (" + result + ")"; }

  std::string get_host() const { return host; }
  std::string get_addresses() const { return addresses; }
  std::string get_type() const { return type; }
  std::string get_server() const { return server; }
  std::string get_result() const { return result; }
  long long get_count() const { return count; }
  long long get_time() const { return time; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_dns_filter

void check_dns(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check_net
