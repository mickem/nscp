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

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace check_net {
namespace check_ntp_filter {

struct filter_obj {
  std::string server;
  long long port;
  // Offset between the local clock and the server clock, in milliseconds.
  // Positive means local clock is ahead of the server.
  long long offset_ms;
  long long stratum;
  long long time;  // round trip time, ms
  std::string result;

  filter_obj() : port(0), offset_ms(0), stratum(0), time(0) {}

  std::string show() const { return server + " offset=" + std::to_string(offset_ms) + "ms stratum=" + std::to_string(stratum) + " (" + result + ")"; }

  std::string get_server() const { return server; }
  long long get_port() const { return port; }
  // Magnitude of the offset, useful for "offset > 1000" thresholds.
  long long get_offset() const { return offset_ms < 0 ? -offset_ms : offset_ms; }
  long long get_offset_signed() const { return offset_ms; }
  long long get_stratum() const { return stratum; }
  long long get_time() const { return time; }
  std::string get_result() const { return result; }
};

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_ntp_filter

void check_ntp_offset(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check_net
